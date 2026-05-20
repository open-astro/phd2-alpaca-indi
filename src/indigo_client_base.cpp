/*
 *  indigo_client_base.cpp
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

#ifdef HAVE_INDIGO

# include "indigo_client_base.h"

# include <mutex>
# include <string.h>

namespace
{
// indigo_start() / indigo_stop() are process-global. Refcount them so multiple
// drivers (camera + scope + rotator on the same profile) share one bus.
std::mutex s_busLock;
int s_busRefcount = 0;
} // namespace

IndigoBusGuard::IndigoBusGuard()
{
    std::lock_guard<std::mutex> lock(s_busLock);
    if (s_busRefcount++ == 0)
    {
        indigo_start();
    }
}

IndigoBusGuard::~IndigoBusGuard()
{
    std::lock_guard<std::mutex> lock(s_busLock);
    if (--s_busRefcount == 0)
    {
        indigo_stop();
    }
}

IndigoClientBase::IndigoClientBase(const char *clientName)
{
    memset(&m_client, 0, sizeof(m_client));
    // INDIGO_NAME_SIZE is the upper bound; strncpy + explicit terminator keeps
    // us safe against an oversized clientName without dragging in extra deps.
    strncpy(m_client.name, clientName ? clientName : "phd2", sizeof(m_client.name) - 1);
    m_client.version = INDIGO_VERSION_CURRENT;
    m_client.client_context = this;
    m_client.attach = &IndigoClientBase::Thunk_Attach;
    m_client.define_property = &IndigoClientBase::Thunk_DefineProperty;
    m_client.update_property = &IndigoClientBase::Thunk_UpdateProperty;
    m_client.delete_property = &IndigoClientBase::Thunk_DeleteProperty;
    m_client.send_message = &IndigoClientBase::Thunk_SendMessage;
    m_client.detach = &IndigoClientBase::Thunk_Detach;
    indigo_attach_client(&m_client);
}

IndigoClientBase::~IndigoClientBase()
{
    DisconnectServer();
    indigo_detach_client(&m_client);
}

bool IndigoClientBase::ConnectServer(const char *name, const wxString& host, int port)
{
    if (m_server)
        return false;
    indigo_result rc = indigo_connect_server(name, host.mb_str(wxConvUTF8), port, &m_server);
    if (rc != INDIGO_OK)
    {
        m_server = nullptr;
        return false;
    }
    return true;
}

void IndigoClientBase::DisconnectServer()
{
    if (!m_server)
        return;
    indigo_disconnect_server(m_server);
    m_server = nullptr;
}

void IndigoClientBase::EnumerateAllProperties()
{
    indigo_enumerate_properties(&m_client, &INDIGO_ALL_PROPERTIES);
}

// Static thunks. INDIGO stores our `this` pointer in client_context; cast back
// and dispatch. The `client` argument is always &m_client of the same object,
// so we don't have to disambiguate.
indigo_result IndigoClientBase::Thunk_Attach(indigo_client *client)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnAttach();
}

indigo_result IndigoClientBase::Thunk_DefineProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                                     const char *message)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnDefineProperty(device, property, message);
}

indigo_result IndigoClientBase::Thunk_UpdateProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                                     const char *message)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnUpdateProperty(device, property, message);
}

indigo_result IndigoClientBase::Thunk_DeleteProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                                     const char *message)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnDeleteProperty(device, property, message);
}

indigo_result IndigoClientBase::Thunk_SendMessage(indigo_client *client, indigo_device *device, const char *message)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnSendMessage(device, message);
}

indigo_result IndigoClientBase::Thunk_Detach(indigo_client *client)
{
    return static_cast<IndigoClientBase *>(client->client_context)->OnDetach();
}

#endif // HAVE_INDIGO
