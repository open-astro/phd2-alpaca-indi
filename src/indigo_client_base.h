/*
 *  indigo_client_base.h
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

#ifndef INDIGO_CLIENT_BASE_INCLUDED
#define INDIGO_CLIENT_BASE_INCLUDED

#ifdef HAVE_INDIGO

# include <indigo/indigo_bus.h>
# include <indigo/indigo_client.h>

# include <wx/string.h>

// IndigoBusGuard refcounts indigo_start() / indigo_stop(). The INDIGO bus is
// process-global; calling indigo_start() twice from separate clients leaks the
// internal thread/queue state if not balanced by matching indigo_stop()s.
// Each driver instance (CameraIndigo, ScopeIndigo, RotatorIndigo) holds one
// guard; the bus stays up while any guard is alive and shuts down on the
// last-out destructor.
class IndigoBusGuard
{
public:
    IndigoBusGuard();
    ~IndigoBusGuard();

    IndigoBusGuard(const IndigoBusGuard&) = delete;
    IndigoBusGuard& operator=(const IndigoBusGuard&) = delete;
};

// IndigoClientBase wraps INDIGO's C `indigo_client` callback struct in a
// virtual-dispatch C++ class. INDIGO's bus invokes plain C function pointers
// that take an `indigo_client *`; we set `client_context = this` and use
// static thunks to forward each callback to the matching virtual method on
// the C++ instance.
//
// Subclass and override the On*() hooks to react to the INDIGO bus. Bus
// callbacks run on INDIGO's internal thread, so subclasses must lock any
// state they share with the PHD2 main thread (compare INDI::BaseClient, which
// makes the same threading promise).
class IndigoClientBase
{
public:
    explicit IndigoClientBase(const char *clientName);
    virtual ~IndigoClientBase();

    IndigoClientBase(const IndigoClientBase&) = delete;
    IndigoClientBase& operator=(const IndigoClientBase&) = delete;

    // Open a TCP connection to indigo_server at host:port. Returns true on
    // success; on success the bus begins streaming property defines/updates
    // through the OnDefineProperty / OnUpdateProperty hooks. The `name`
    // argument is the local handle INDIGO uses for the connection (visible
    // in indigo_server logs) and is not network-visible to peers.
    bool ConnectServer(const char *name, const wxString& host, int port);
    void DisconnectServer();
    bool IsServerConnected() const { return m_server != nullptr; }

    // Ask the server to broadcast every property it knows about. The replies
    // arrive asynchronously through OnDefineProperty.
    void EnumerateAllProperties();

    // Property-setting helpers. INDIGO's set-by-name calls dispatch to the
    // server on whatever thread the caller is on (no waiting); responses
    // arrive later through OnUpdateProperty. The boolean return only reflects
    // whether the request was queued — actual success/failure shows up as a
    // property-state change in OnUpdateProperty.
    bool SetSwitch1(const char *device, const char *property, const char *item, bool value);
    bool SetNumber1(const char *device, const char *property, const char *item, double value);
    bool SetText1(const char *device, const char *property, const char *item, const char *value);

    // Convenience: flip the CONNECTION property's CONNECTED / DISCONNECTED
    // switch items. The device's connect handshake completes asynchronously
    // through the CONNECTION property's state transitions, which subclasses
    // observe in OnUpdateProperty.
    bool ConnectDevice(const char *device);
    bool DisconnectDevice(const char *device);

    // Find an item by name inside a property's items[] array. Returns nullptr
    // if no match. INDIGO has no shipped equivalent (compare INDI's
    // IUFindNumber / IUFindSwitch) so we provide one here.
    static indigo_item *FindItem(indigo_property *property, const char *name);

protected:
    // Bus callback hooks. Default implementations return INDIGO_OK so a
    // subclass only has to override the ones it cares about.
    virtual indigo_result OnAttach() { return INDIGO_OK; }
    virtual indigo_result OnDefineProperty(indigo_device *device, indigo_property *property, const char *message)
    {
        return INDIGO_OK;
    }
    virtual indigo_result OnUpdateProperty(indigo_device *device, indigo_property *property, const char *message)
    {
        return INDIGO_OK;
    }
    virtual indigo_result OnDeleteProperty(indigo_device *device, indigo_property *property, const char *message)
    {
        return INDIGO_OK;
    }
    virtual indigo_result OnSendMessage(indigo_device *device, const char *message) { return INDIGO_OK; }
    virtual indigo_result OnDetach() { return INDIGO_OK; }

    // Direct access to the underlying indigo_client for property-change calls
    // (indigo_change_*_property, indigo_device_connect, indigo_enable_blob).
    indigo_client *Client() { return &m_client; }

private:
    IndigoBusGuard m_busGuard;
    indigo_client m_client;
    indigo_server_entry *m_server = nullptr;

    // Static C thunks that route bus callbacks to virtual methods on the
    // instance stored in m_client.client_context.
    static indigo_result Thunk_Attach(indigo_client *client);
    static indigo_result Thunk_DefineProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                              const char *message);
    static indigo_result Thunk_UpdateProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                              const char *message);
    static indigo_result Thunk_DeleteProperty(indigo_client *client, indigo_device *device, indigo_property *property,
                                              const char *message);
    static indigo_result Thunk_SendMessage(indigo_client *client, indigo_device *device, const char *message);
    static indigo_result Thunk_Detach(indigo_client *client);
};

#endif // HAVE_INDIGO

#endif // INDIGO_CLIENT_BASE_INCLUDED
