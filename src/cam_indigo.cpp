/*
 *  cam_indigo.cpp
 *  PHD Guiding
 *
 *  Copyright (c) 2026 openastro-phd2 contributors
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of openastro-phd2 nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "phd.h"

#ifdef INDIGO_CAMERA

# include "cam_indigo.h"
# include "camera.h"
# include "indigo_client_base.h"
# include "config_indigo.h"
# include "image_math.h"
# include "fitsiowrap.h"

# include <atomic>
# include <condition_variable>
# include <mutex>
# include <vector>

# include <fitsio.h>
# include <indigo/indigo_names.h>

class CameraINDIGO : public GuideCamera, public IndigoClientBase
{
public:
    CameraINDIGO();
    ~CameraINDIGO() override;

    bool Connect(const wxString& camId) override;
    bool Disconnect() override;
    bool Capture(usImage& img, const CaptureParams& captureParams) override;

    bool HasNonGuiCapture() override { return true; }
    wxByte BitsPerPixel() override { return 16; }
    bool GetDevicePixelSize(double *devPixelSize) override;
    void ShowPropertyDialog() override;

protected:
    indigo_result OnDefineProperty(indigo_device *device, indigo_property *property, const char *message) override;
    indigo_result OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message) override;

private:
    wxString m_host;
    long m_port;
    wxString m_devName;

    // Bus-thread state. m_lock guards the connection flag, the captured BLOB,
    // and the device-info atomics' "seen yet?" flags; m_cond wakes the
    // foreground Connect() and Capture() loops on state changes.
    std::mutex m_lock;
    std::condition_variable m_cond;
    bool m_deviceConnected = false;
    bool m_imageReady = false;
    std::vector<unsigned char> m_imageData;
    char m_imageFormat[16] = { 0 };

    std::atomic<bool> m_hasExposure { false };
    std::atomic<bool> m_hasImage { false };
    std::atomic<double> m_pixelSize { 0.0 };

    bool DeviceMatches(const indigo_property *property) const;
    bool ReadFITS(const std::vector<unsigned char>& data, usImage& img);
    void Setup();
};

void CameraINDIGO::Setup()
{
    INDIGOConfig dlg(wxGetApp().GetTopWindow(), _("INDIGO Camera Selection"));
    dlg.host = m_host;
    dlg.port = m_port;
    dlg.devName = m_devName;
    dlg.SetSettings();
    if (dlg.ShowModal() != wxID_OK)
        return;
    dlg.SaveSettings();
    m_host = dlg.host;
    m_port = dlg.port;
    m_devName = dlg.devName;
    pConfig->Profile.SetString("/indigo/host", m_host);
    pConfig->Profile.SetLong("/indigo/port", m_port);
    pConfig->Profile.SetString("/indigo/camera", m_devName);
    Name = m_devName.empty() ? wxString(_T("INDIGO Camera")) : wxString::Format(_T("INDIGO Camera [%s]"), m_devName);
}

void CameraINDIGO::ShowPropertyDialog()
{
    Setup();
}

CameraINDIGO::CameraINDIGO() : IndigoClientBase("phd2-camera")
{
    Connected = false;
    HasGainControl = false;
    HasSubframes = false;
    HasCooler = false;
    PropertyDialogType = PROPDLG_ANY;

    m_host = pConfig->Profile.GetString("/indigo/host", _T("localhost"));
    m_port = pConfig->Profile.GetLong("/indigo/port", 7624);
    m_devName = pConfig->Profile.GetString("/indigo/camera", wxEmptyString);
    Name = m_devName.empty() ? wxString(_T("INDIGO Camera")) : wxString::Format(_T("INDIGO Camera [%s]"), m_devName);
}

CameraINDIGO::~CameraINDIGO()
{
    DisconnectServer();
}

bool CameraINDIGO::DeviceMatches(const indigo_property *property) const
{
    if (!property || m_devName.empty())
        return false;
    return m_devName.IsSameAs(wxString::FromUTF8(property->device));
}

bool CameraINDIGO::Connect(const wxString& camId)
{
    if (m_devName.empty())
        Setup();
    if (m_devName.empty())
    {
        Debug.Write(_T("INDIGO Camera: no device name configured; declining to connect\n"));
        return true;
    }

    if (!ConnectServer("phd2-camera", m_host, m_port))
    {
        Debug.Write(wxString::Format("INDIGO Camera: connect to %s:%ld failed\n", m_host, m_port));
        return true;
    }

    struct ConnectInBg : public ConnectCameraInBg
    {
        CameraINDIGO *self;
        explicit ConnectInBg(CameraINDIGO *s) : self(s) { }
        bool Entry() override
        {
            std::unique_lock<std::mutex> lock(self->m_lock);
            for (int i = 0; i < 300; ++i)
            {
                if (self->m_deviceConnected)
                    return false;
                if (IsCanceled())
                    return true;
                self->m_cond.wait_for(lock, std::chrono::milliseconds(100));
            }
            return true;
        }
    };

    if (ConnectInBg(this).Run())
    {
        DisconnectServer();
        return true;
    }

    Connected = true;
    return false;
}

bool CameraINDIGO::Disconnect()
{
    if (!m_devName.empty())
        IndigoClientBase::DisconnectDevice(m_devName.mb_str(wxConvUTF8));
    DisconnectServer();
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_deviceConnected = false;
        m_imageReady = false;
        m_imageData.clear();
    }
    Connected = false;
    return false;
}

bool CameraINDIGO::GetDevicePixelSize(double *devPixelSize)
{
    double sz = m_pixelSize.load(std::memory_order_relaxed);
    if (sz <= 0.0)
        return true;
    *devPixelSize = sz;
    return false;
}

bool CameraINDIGO::Capture(usImage& img, const CaptureParams& captureParams)
{
    if (!Connected)
        return true;
    if (!m_hasExposure.load(std::memory_order_relaxed) || !m_hasImage.load(std::memory_order_relaxed))
    {
        Debug.Write("INDIGO Camera: CCD_EXPOSURE / CCD_IMAGE not yet advertised\n");
        return true;
    }

    int duration = captureParams.duration;

    // Drop any prior BLOB residue, then arm the trigger.
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_imageReady = false;
        m_imageData.clear();
        m_imageFormat[0] = 0;
    }

    SetNumber1(m_devName.mb_str(wxConvUTF8), CCD_EXPOSURE_PROPERTY_NAME, CCD_EXPOSURE_ITEM_NAME,
               static_cast<double>(duration) / 1000.0);

    CameraWatchdog watchdog(duration, GetTimeoutMs());
    std::unique_lock<std::mutex> lock(m_lock);
    while (!m_imageReady)
    {
        m_cond.wait_for(lock, std::chrono::milliseconds(100));
        if (WorkerThread::TerminateRequested())
            return true;
        if (watchdog.Expired())
        {
            DisconnectWithAlert(CAPT_FAIL_TIMEOUT);
            return true;
        }
    }

    // Take ownership of the buffer so we can release the lock during the
    // FITS parse and let the next bus-thread BLOB delivery start populating
    // a fresh m_imageData behind us.
    std::vector<unsigned char> data;
    data.swap(m_imageData);
    wxString format = wxString::FromAscii(m_imageFormat);
    m_imageReady = false;
    lock.unlock();

    if (!format.IsSameAs(_T(".fits")))
    {
        Debug.Write(wxString::Format("INDIGO Camera: unsupported image format '%s' (need .fits)\n", format));
        return true;
    }

    return ReadFITS(data, img);
}

bool CameraINDIGO::ReadFITS(const std::vector<unsigned char>& data, usImage& img)
{
    fitsfile *fptr = nullptr;
    int status = 0;

    void *buf = const_cast<unsigned char *>(data.data());
    size_t size = data.size();
    if (fits_open_memfile(&fptr, "", READONLY, &buf, &size, 0, nullptr, &status))
    {
        pFrame->Alert(_("Unsupported type or read error loading FITS file"));
        return true;
    }

    int hdutype;
    if (fits_get_hdu_type(fptr, &hdutype, &status) || hdutype != IMAGE_HDU)
    {
        pFrame->Alert(_("FITS file is not of an image"));
        PHD_fits_close_file(fptr);
        return true;
    }

    int naxis = 0;
    fits_get_img_dim(fptr, &naxis, &status);
    long fits_size[2] = { 0, 0 };
    fits_get_img_size(fptr, 2, fits_size, &status);
    int xsize = static_cast<int>(fits_size[0]);
    int ysize = static_cast<int>(fits_size[1]);

    if (naxis == 3)
    {
        pFrame->Alert(_("RGB images are not supported, please switch the INDIGO driver to Mono"));
        PHD_fits_close_file(fptr);
        return true;
    }
    if (naxis != 2)
    {
        pFrame->Alert(_("Unsupported FITS file"));
        PHD_fits_close_file(fptr);
        return true;
    }

    FrameSize.Set(xsize, ysize);
    if (img.Init(FrameSize))
    {
        pFrame->Alert(_("Memory allocation error"));
        PHD_fits_close_file(fptr);
        return true;
    }

    long fpixel[3] = { 1, 1, 1 };
    if (fits_read_pix(fptr, TUSHORT, fpixel, xsize * ysize, nullptr, img.ImageData, nullptr, &status))
    {
        pFrame->Alert(_("Error reading data"));
        PHD_fits_close_file(fptr);
        return true;
    }

    PHD_fits_close_file(fptr);
    return false;
}

indigo_result CameraINDIGO::OnDefineProperty(indigo_device *device, indigo_property *property, const char *message)
{
    if (!DeviceMatches(property))
        return INDIGO_OK;

    if (strcmp(property->name, CONNECTION_PROPERTY_NAME) == 0)
    {
        if (!indigo_get_switch(property, CONNECTION_CONNECTED_ITEM_NAME))
            IndigoClientBase::ConnectDevice(property->device);
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_EXPOSURE_PROPERTY_NAME) == 0)
    {
        m_hasExposure.store(true, std::memory_order_relaxed);
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_IMAGE_PROPERTY_NAME) == 0)
    {
        // BLOBs are URL-only by default for remote servers; flip to inline so
        // the bus actually streams the bytes we need to assemble a FITS image.
        indigo_enable_blob(Client(), property, INDIGO_ENABLE_BLOB_ALSO);
        m_hasImage.store(true, std::memory_order_relaxed);
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_IMAGE_FORMAT_PROPERTY_NAME) == 0)
    {
        // Force FITS — that's what we parse on the receiving end.
        SetSwitch1(property->device, CCD_IMAGE_FORMAT_PROPERTY_NAME, CCD_IMAGE_FORMAT_FITS_ITEM_NAME, true);
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_INFO_PROPERTY_NAME) == 0)
    {
        if (indigo_item *item = FindItem(property, CCD_INFO_PIXEL_SIZE_ITEM_NAME))
            m_pixelSize.store(item->number.value, std::memory_order_relaxed);
        return INDIGO_OK;
    }
    return INDIGO_OK;
}

indigo_result CameraINDIGO::OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message)
{
    if (!DeviceMatches(property))
        return INDIGO_OK;

    if (strcmp(property->name, CONNECTION_PROPERTY_NAME) == 0)
    {
        bool connected = indigo_get_switch(property, CONNECTION_CONNECTED_ITEM_NAME) && property->state == INDIGO_OK_STATE;
        std::lock_guard<std::mutex> lock(m_lock);
        if (connected != m_deviceConnected)
        {
            m_deviceConnected = connected;
            m_cond.notify_all();
        }
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_IMAGE_PROPERTY_NAME) == 0 && property->state == INDIGO_OK_STATE && property->count > 0)
    {
        indigo_item *item = &property->items[0];
        if (item->blob.value && item->blob.size > 0)
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_imageData.assign(static_cast<unsigned char *>(item->blob.value),
                               static_cast<unsigned char *>(item->blob.value) + item->blob.size);
            strncpy(m_imageFormat, item->blob.format, sizeof(m_imageFormat) - 1);
            m_imageReady = true;
            m_cond.notify_all();
        }
        return INDIGO_OK;
    }
    if (strcmp(property->name, CCD_INFO_PROPERTY_NAME) == 0)
    {
        if (indigo_item *item = FindItem(property, CCD_INFO_PIXEL_SIZE_ITEM_NAME))
            m_pixelSize.store(item->number.value, std::memory_order_relaxed);
    }
    return INDIGO_OK;
}

GuideCamera *INDIGOCameraFactory::MakeINDIGOCamera()
{
    return new CameraINDIGO();
}

#endif // INDIGO_CAMERA
