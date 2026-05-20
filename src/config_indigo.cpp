/*
 *  config_indigo.cpp
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

# include "config_indigo.h"

# include <wx/sizer.h>
# include <wx/stattext.h>
# include <wx/textctrl.h>
# include <wx/spinctrl.h>

INDIGOConfig::INDIGOConfig(wxWindow *parent, const wxString& title)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE), host(_T("localhost")),
      port(7624)
{
    auto *grid = new wxFlexGridSizer(2, 5, 5);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Host:")), 0, wxALIGN_CENTER_VERTICAL);
    m_hostCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(220, -1));
    grid->Add(m_hostCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Port:")), 0, wxALIGN_CENTER_VERTICAL);
    m_portCtrl =
        new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 65535, 7624);
    grid->Add(m_portCtrl, 0);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Device name:")), 0, wxALIGN_CENTER_VERTICAL);
    m_devCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(220, -1));
    grid->Add(m_devCtrl, 1, wxEXPAND);

    auto *help = new wxStaticText(this, wxID_ANY,
                                  _("Enter the INDIGO server host and port, plus the device name as it appears\n"
                                    "in indigo_server (e.g. 'CCD Imager Simulator @ indigosky')."));

    auto *buttons = CreateButtonSizer(wxOK | wxCANCEL);

    auto *outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(grid, 0, wxEXPAND | wxALL, 10);
    outer->Add(help, 0, wxALL, 10);
    if (buttons)
        outer->Add(buttons, 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(outer);
}

void INDIGOConfig::SetSettings()
{
    m_hostCtrl->SetValue(host);
    m_portCtrl->SetValue(static_cast<int>(port));
    m_devCtrl->SetValue(devName);
}

void INDIGOConfig::SaveSettings()
{
    host = m_hostCtrl->GetValue();
    port = m_portCtrl->GetValue();
    devName = m_devCtrl->GetValue();
}

#endif // HAVE_INDIGO
