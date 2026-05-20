/*
 *  scope_indigo.cpp
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

#ifdef GUIDE_INDIGO

# include "scope_indigo.h"
# include "scope.h"
# include "indigo_client_base.h"
# include "runinbg.h"

# include <atomic>
# include <condition_variable>
# include <mutex>

# include <indigo/indigo_names.h>

class ScopeINDIGO : public Scope, public IndigoClientBase
{
public:
    ScopeINDIGO();
    ~ScopeINDIGO() override;

    bool Connect() override;
    bool Disconnect() override;

    MOVE_RESULT Guide(GUIDE_DIRECTION direction, int duration) override;
    bool HasNonGuiMove() override { return true; }
    bool CanPulseGuide() override { return m_hasPulseRA && m_hasPulseDec; }
    bool CanReportPosition() override { return m_hasCoord; }
    bool GetCoordinates(double *ra, double *dec, double *siderealTime) override;
    double GetDeclinationRadians() override;

protected:
    indigo_result OnDefineProperty(indigo_device *device, indigo_property *property, const char *message) override;
    indigo_result OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message) override;

private:
    wxString m_host;
    long m_port;
    wxString m_devName;
    wxString m_displayName;

    // Bus-thread state. m_lock guards m_deviceConnected and the pulse-guide
    // active flag; m_cond wakes the foreground Connect()/Guide() loops when
    // those flip. RA/Dec are stored atomically so the guider main loop's
    // GetCoordinates() poll doesn't need to acquire the lock.
    mutable std::mutex m_lock;
    std::condition_variable m_cond;
    bool m_deviceConnected = false;
    bool m_guideActive = false;
    GuideAxis m_guideAxis = GUIDE_RA;
    bool m_hasPulseRA = false;
    bool m_hasPulseDec = false;
    bool m_hasCoord = false;
    std::atomic<double> m_ra { 0.0 };
    std::atomic<double> m_dec { UNKNOWN_DECLINATION };

    bool DeviceMatches(const indigo_property *property) const;
};

ScopeINDIGO::ScopeINDIGO() : IndigoClientBase("phd2-mount")
{
    m_host = pConfig->Profile.GetString("/indigo/host", _T("localhost"));
    m_port = pConfig->Profile.GetLong("/indigo/port", 7624);
    m_devName = pConfig->Profile.GetString("/indigo/mount", wxEmptyString);
    m_displayName = m_devName.empty() ? wxString(_T("INDIGO Mount")) : wxString::Format("INDIGO Mount [%s]", m_devName);
}

ScopeINDIGO::~ScopeINDIGO()
{
    DisconnectServer();
}

bool ScopeINDIGO::DeviceMatches(const indigo_property *property) const
{
    if (!property || m_devName.empty())
        return false;
    return m_devName.IsSameAs(wxString::FromUTF8(property->device));
}

bool ScopeINDIGO::Connect()
{
    if (m_devName.empty())
    {
        Debug.Write(_T("INDIGO Mount: no device name configured; declining to connect\n"));
        return true;
    }

    if (!ConnectServer("phd2-mount", m_host, m_port))
    {
        Debug.Write(wxString::Format("INDIGO Mount: connect to %s:%ld failed\n", m_host, m_port));
        return true;
    }

    struct ConnectInBg : public ConnectMountInBg
    {
        ScopeINDIGO *self;
        explicit ConnectInBg(ScopeINDIGO *s) : self(s) { }
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

    Scope::Connect();
    return false;
}

bool ScopeINDIGO::Disconnect()
{
    if (!m_devName.empty())
        IndigoClientBase::DisconnectDevice(m_devName.mb_str(wxConvUTF8));
    DisconnectServer();
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_deviceConnected = false;
        m_guideActive = false;
        m_hasPulseRA = m_hasPulseDec = m_hasCoord = false;
        m_dec.store(UNKNOWN_DECLINATION, std::memory_order_relaxed);
    }
    Scope::Disconnect();
    return false;
}

// INDIGO pulse-guide properties: GUIDER_GUIDE_RA with EAST/WEST items, and
// GUIDER_GUIDE_DEC with NORTH/SOUTH items. Each item value is the pulse
// duration in milliseconds; the property's state goes BUSY → OK when the
// pulse completes.
Mount::MOVE_RESULT ScopeINDIGO::Guide(GUIDE_DIRECTION direction, int duration)
{
    if (!m_hasPulseRA || !m_hasPulseDec)
    {
        Debug.Write("INDIGO Mount: pulse-guide properties unavailable\n");
        return MOVE_ERROR;
    }

    const char *prop = nullptr;
    const char *item = nullptr;
    GuideAxis axis = GUIDE_RA;
    switch (direction)
    {
    case EAST:
        prop = GUIDER_GUIDE_RA_PROPERTY_NAME;
        item = GUIDER_GUIDE_EAST_ITEM_NAME;
        axis = GUIDE_RA;
        break;
    case WEST:
        prop = GUIDER_GUIDE_RA_PROPERTY_NAME;
        item = GUIDER_GUIDE_WEST_ITEM_NAME;
        axis = GUIDE_RA;
        break;
    case NORTH:
        prop = GUIDER_GUIDE_DEC_PROPERTY_NAME;
        item = GUIDER_GUIDE_NORTH_ITEM_NAME;
        axis = GUIDE_DEC;
        break;
    case SOUTH:
        prop = GUIDER_GUIDE_DEC_PROPERTY_NAME;
        item = GUIDER_GUIDE_SOUTH_ITEM_NAME;
        axis = GUIDE_DEC;
        break;
    default:
        Debug.Write("INDIGO Mount: Guide called with NONE direction\n");
        return MOVE_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_guideActive)
        {
            Debug.Write("INDIGO Mount: cannot guide while another pulse is in progress\n");
            return MOVE_ERROR;
        }
        m_guideActive = true;
        m_guideAxis = axis;
    }

    SetNumber1(m_devName.mb_str(wxConvUTF8), prop, item, static_cast<double>(duration));

    // Wait for the pulse-guide property to leave INDIGO_BUSY_STATE, which
    // OnUpdateProperty will report by clearing m_guideActive and notifying.
    std::unique_lock<std::mutex> lock(m_lock);
    while (m_guideActive)
    {
        m_cond.wait_for(lock, std::chrono::milliseconds(100));
        if (WorkerThread::InterruptRequested())
        {
            Debug.Write("INDIGO Mount: pulse-guide interrupted\n");
            m_guideActive = false;
            return MOVE_ERROR;
        }
    }
    return MOVE_OK;
}

bool ScopeINDIGO::GetCoordinates(double *ra, double *dec, double *siderealTime)
{
    if (!m_hasCoord)
        return true;
    *ra = m_ra.load(std::memory_order_relaxed);
    *dec = m_dec.load(std::memory_order_relaxed);
    *siderealTime = 0.0;
    return false;
}

double ScopeINDIGO::GetDeclinationRadians()
{
    if (!m_hasCoord)
        return UNKNOWN_DECLINATION;
    double dec = m_dec.load(std::memory_order_relaxed);
    if (dec > 89.0)
        dec = 89.0;
    if (dec < -89.0)
        dec = -89.0;
    return radians(dec);
}

indigo_result ScopeINDIGO::OnDefineProperty(indigo_device *device, indigo_property *property, const char *message)
{
    if (!DeviceMatches(property))
        return INDIGO_OK;

    if (strcmp(property->name, CONNECTION_PROPERTY_NAME) == 0)
    {
        if (!indigo_get_switch(property, CONNECTION_CONNECTED_ITEM_NAME))
            IndigoClientBase::ConnectDevice(property->device);
        return INDIGO_OK;
    }
    if (strcmp(property->name, GUIDER_GUIDE_RA_PROPERTY_NAME) == 0)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_hasPulseRA = true;
    }
    else if (strcmp(property->name, GUIDER_GUIDE_DEC_PROPERTY_NAME) == 0)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_hasPulseDec = true;
    }
    else if (strcmp(property->name, MOUNT_EQUATORIAL_COORDINATES_PROPERTY_NAME) == 0)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_hasCoord = true;
        if (indigo_item *ra = FindItem(property, MOUNT_EQUATORIAL_COORDINATES_RA_ITEM_NAME))
            m_ra.store(ra->number.value, std::memory_order_relaxed);
        if (indigo_item *dec = FindItem(property, MOUNT_EQUATORIAL_COORDINATES_DEC_ITEM_NAME))
            m_dec.store(dec->number.value, std::memory_order_relaxed);
    }
    return INDIGO_OK;
}

indigo_result ScopeINDIGO::OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message)
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
    // Pulse-guide completion: when the BUSY → !BUSY transition happens on the
    // axis we're guiding, wake the Guide() waiter.
    if (strcmp(property->name, GUIDER_GUIDE_RA_PROPERTY_NAME) == 0 ||
        strcmp(property->name, GUIDER_GUIDE_DEC_PROPERTY_NAME) == 0)
    {
        bool isRA = strcmp(property->name, GUIDER_GUIDE_RA_PROPERTY_NAME) == 0;
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_guideActive && property->state != INDIGO_BUSY_STATE &&
            ((m_guideAxis == GUIDE_RA && isRA) || (m_guideAxis == GUIDE_DEC && !isRA)))
        {
            m_guideActive = false;
            m_cond.notify_all();
        }
        return INDIGO_OK;
    }
    if (strcmp(property->name, MOUNT_EQUATORIAL_COORDINATES_PROPERTY_NAME) == 0)
    {
        if (indigo_item *ra = FindItem(property, MOUNT_EQUATORIAL_COORDINATES_RA_ITEM_NAME))
            m_ra.store(ra->number.value, std::memory_order_relaxed);
        if (indigo_item *dec = FindItem(property, MOUNT_EQUATORIAL_COORDINATES_DEC_ITEM_NAME))
            m_dec.store(dec->number.value, std::memory_order_relaxed);
    }
    return INDIGO_OK;
}

Scope *INDIGOScopeFactory::MakeINDIGOScope()
{
    return new ScopeINDIGO();
}

#endif // GUIDE_INDIGO
