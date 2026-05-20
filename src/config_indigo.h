/*
 *  config_indigo.h
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

#ifndef CONFIG_INDIGO_INCLUDED
#define CONFIG_INDIGO_INCLUDED

#include <wx/dialog.h>
#include <wx/string.h>

class wxTextCtrl;
class wxSpinCtrl;
class wxWindow;
class wxCommandEvent;

// Minimal text-entry dialog for INDIGO server + device selection. The dialog
// has no notion of which kind of device is being configured — the caller
// pre-fills `host`/`port`/`devName` from its own pConfig key namespace
// (`/indigo/camera`, `/indigo/mount`, `/indigo/rotator`) and reads them back
// after ShowModal returns wxID_OK.
//
// Discovery (mDNS via INDIGO's indigo_service_discovery) and an INDIGO
// property tree browser parallel to indi_gui.cpp are deliberately out of
// scope here; this is the minimum surface that lets a user configure a
// driver without editing pConfig by hand.
class INDIGOConfig : public wxDialog
{
public:
    INDIGOConfig(wxWindow *parent, const wxString& title);
    ~INDIGOConfig() override = default;

    wxString host;
    long port;
    wxString devName;

    // Push the host/port/devName fields above into the dialog controls.
    // Call before ShowModal.
    void SetSettings();

    // Read the dialog controls back into host/port/devName after ShowModal
    // returns wxID_OK.
    void SaveSettings();

private:
    wxTextCtrl *m_hostCtrl;
    wxSpinCtrl *m_portCtrl;
    wxTextCtrl *m_devCtrl;
};

#endif // CONFIG_INDIGO_INCLUDED
