/*
 *  rotator_indigo.cpp
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

#ifdef ROTATOR_INDIGO

# include "rotator_indigo.h"
# include "rotator.h"
# include "indigo_client_base.h"
# include "config_indigo.h"
# include "runinbg.h"

# include <atomic>
# include <condition_variable>
# include <mutex>

# include <indigo/indigo_names.h>

class RotatorINDIGO : public Rotator, public IndigoClientBase
{
public:
    RotatorINDIGO();
    ~RotatorINDIGO() override;

    bool Connect() override;
    bool Disconnect() override;
    wxString Name() const override;
    float Position() const override;
    void ShowPropertyDialog() override;

protected:
    indigo_result OnDefineProperty(indigo_device *device, indigo_property *property, const char *message) override;
    indigo_result OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message) override;

private:
    wxString m_host;
    long m_port;
    wxString m_devName;
    wxString m_displayName;

    // Bus-thread state. m_lock guards m_deviceConnected; m_cond pings the
    // foreground Connect() poll loop when it changes. m_angle is the live
    // rotator position; std::atomic keeps the foreground Position() call
    // lock-free since it can be read on every frame.
    mutable std::mutex m_lock;
    std::condition_variable m_cond;
    bool m_deviceConnected = false;
    std::atomic<float> m_angle { POSITION_UNKNOWN };

    bool DeviceMatches(const indigo_property *property) const;
    void Setup();
};

void RotatorINDIGO::Setup()
{
    INDIGOConfig dlg(wxGetApp().GetTopWindow(), _("INDIGO Rotator Selection"));
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
    pConfig->Profile.SetString("/indigo/rotator", m_devName);
    m_displayName = m_devName.empty() ? wxString(_T("INDIGO Rotator")) : wxString::Format("INDIGO Rotator [%s]", m_devName);
}

void RotatorINDIGO::ShowPropertyDialog()
{
    Setup();
}

RotatorINDIGO::RotatorINDIGO() : IndigoClientBase("phd2-rotator")
{
    m_host = pConfig->Profile.GetString("/indigo/host", _T("localhost"));
    m_port = pConfig->Profile.GetLong("/indigo/port", 7624);
    m_devName = pConfig->Profile.GetString("/indigo/rotator", wxEmptyString);
    m_displayName = m_devName.empty() ? wxString(_T("INDIGO Rotator")) : wxString::Format("INDIGO Rotator [%s]", m_devName);
}

RotatorINDIGO::~RotatorINDIGO()
{
    DisconnectServer();
}

wxString RotatorINDIGO::Name() const
{
    return m_displayName;
}

float RotatorINDIGO::Position() const
{
    return m_angle.load(std::memory_order_relaxed);
}

bool RotatorINDIGO::DeviceMatches(const indigo_property *property) const
{
    if (!property || m_devName.empty())
        return false;
    return m_devName.IsSameAs(wxString::FromUTF8(property->device));
}

bool RotatorINDIGO::Connect()
{
    if (m_devName.empty())
        Setup();
    if (m_devName.empty())
    {
        Debug.Write(_T("INDIGO Rotator: no device name configured; declining to connect\n"));
        return true; // failure
    }

    if (!ConnectServer("phd2-rotator", m_host, m_port))
    {
        Debug.Write(wxString::Format("INDIGO Rotator: connect to %s:%ld failed\n", m_host, m_port));
        return true; // failure
    }

    // The server will start streaming property definitions immediately. Our
    // OnDefineProperty will see CONNECTION for the target device and call
    // ConnectDevice(); OnUpdateProperty will eventually flag m_deviceConnected
    // when the device's CONNECTION.CONNECTED transitions to OK. Wait for that
    // here, on a background thread so the main wx loop keeps pumping.
    struct ConnectInBg : public ConnectRotatorInBg
    {
        RotatorINDIGO *self;
        explicit ConnectInBg(RotatorINDIGO *s) : self(s) { }

        bool Entry() override
        {
            std::unique_lock<std::mutex> lock(self->m_lock);
            for (int i = 0; i < 300; ++i)
            {
                if (self->m_deviceConnected)
                    return false; // success
                if (IsCanceled())
                    return true;
                self->m_cond.wait_for(lock, std::chrono::milliseconds(100));
            }
            return true; // timeout
        }
    };

    if (ConnectInBg(this).Run())
    {
        DisconnectServer();
        return true; // failure
    }

    Rotator::Connect();
    return false; // success
}

bool RotatorINDIGO::Disconnect()
{
    if (!m_devName.empty())
        IndigoClientBase::DisconnectDevice(m_devName.mb_str(wxConvUTF8));
    DisconnectServer();
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_deviceConnected = false;
        m_angle.store(POSITION_UNKNOWN, std::memory_order_relaxed);
    }
    Rotator::Disconnect();
    return false;
}

indigo_result RotatorINDIGO::OnDefineProperty(indigo_device *device, indigo_property *property, const char *message)
{
    if (!DeviceMatches(property))
        return INDIGO_OK;

    // Trigger the device-side connect handshake the first time we see CONNECTION
    // for the target rotator. INDIGO drivers expose CONNECTION as a switch with
    // CONNECTED / DISCONNECTED items.
    if (strcmp(property->name, CONNECTION_PROPERTY_NAME) == 0)
    {
        if (!indigo_get_switch(property, CONNECTION_CONNECTED_ITEM_NAME))
            IndigoClientBase::ConnectDevice(property->device);
        return INDIGO_OK;
    }
    // Snapshot the angle now if the device already exposes a starting value.
    if (strcmp(property->name, ROTATOR_POSITION_PROPERTY_NAME) == 0)
    {
        if (indigo_item *item = FindItem(property, ROTATOR_POSITION_ITEM_NAME))
            m_angle.store(static_cast<float>(item->number.value), std::memory_order_relaxed);
    }
    return INDIGO_OK;
}

indigo_result RotatorINDIGO::OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message)
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
    if (strcmp(property->name, ROTATOR_POSITION_PROPERTY_NAME) == 0)
    {
        if (indigo_item *item = FindItem(property, ROTATOR_POSITION_ITEM_NAME))
            m_angle.store(static_cast<float>(item->number.value), std::memory_order_relaxed);
    }
    return INDIGO_OK;
}

Rotator *INDIGORotatorFactory::MakeINDIGORotator()
{
    return new RotatorINDIGO();
}

#endif // ROTATOR_INDIGO
