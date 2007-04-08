/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <comdef.h>
#include "netfw.h"
#include "winfirewall.h"

#define RELEASE(lpUnk) do \
	{ if ((lpUnk) != NULL) { (lpUnk)->Release(); (lpUnk) = NULL; } } while (0)

namespace cricket {

//////////////////////////////////////////////////////////////////////
// WinFirewall
//////////////////////////////////////////////////////////////////////

WinFirewall::WinFirewall() : mgr_(NULL), policy_(NULL), profile_(NULL) {
}

WinFirewall::~WinFirewall() {
  Shutdown();
}

bool
WinFirewall::Initialize() {
  if (mgr_)
    return true;

  HRESULT hr = CoCreateInstance(__uuidof(NetFwMgr),
                                0, CLSCTX_INPROC_SERVER,
                                __uuidof(INetFwMgr),
                                reinterpret_cast<void **>(&mgr_));
  if (SUCCEEDED(hr) && (mgr_ != NULL))
    hr = mgr_->get_LocalPolicy(&policy_);
  if (SUCCEEDED(hr) && (policy_ != NULL))
    hr = policy_->get_CurrentProfile(&profile_);
  return SUCCEEDED(hr) && (profile_ != NULL);
}

void
WinFirewall::Shutdown() {
  RELEASE(profile_);
  RELEASE(policy_);
  RELEASE(mgr_);
}

bool
WinFirewall::Enabled() {
  if (!profile_)
    return false;

  VARIANT_BOOL fwEnabled = VARIANT_FALSE;
  profile_->get_FirewallEnabled(&fwEnabled);
  return (fwEnabled != VARIANT_FALSE);
}

bool
WinFirewall::Authorized(const char * filename, bool * known) {
  if (known) {
    *known = false;
  }

  if (!profile_)
    return false;

  VARIANT_BOOL fwEnabled = VARIANT_FALSE;
  _bstr_t bfilename = filename;

  INetFwAuthorizedApplications * apps = NULL;
  HRESULT hr = profile_->get_AuthorizedApplications(&apps);
  if (SUCCEEDED(hr) && (apps != NULL)) {
    INetFwAuthorizedApplication * app = NULL;
    hr = apps->Item(bfilename, &app);
    if (SUCCEEDED(hr) && (app != NULL)) {
      hr = app->get_Enabled(&fwEnabled);
      app->Release();
      if (known) {
        *known = true;
      }
    } else if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      // Unexpected error
    }
    apps->Release();
  }

  return (fwEnabled != VARIANT_FALSE);
}

bool
WinFirewall::AddApplication(const char * filename, const char * friendly_name,
                            bool authorized) {
  INetFwAuthorizedApplications * apps = NULL;
  HRESULT hr = profile_->get_AuthorizedApplications(&apps);
  if (SUCCEEDED(hr) && (apps != NULL)) {
    INetFwAuthorizedApplication * app = NULL;
    hr = CoCreateInstance(__uuidof(NetFwAuthorizedApplication),
                          0, CLSCTX_INPROC_SERVER,
                          __uuidof(INetFwAuthorizedApplication),
                          reinterpret_cast<void **>(&app));
    if (SUCCEEDED(hr) && (app != NULL)) {
      _bstr_t bstr = filename;
      hr = app->put_ProcessImageFileName(bstr);
      bstr = friendly_name;
      if (SUCCEEDED(hr))
        hr = app->put_Name(bstr);
      if (SUCCEEDED(hr))
        hr = app->put_Enabled(authorized ? VARIANT_TRUE : VARIANT_FALSE);
      if (SUCCEEDED(hr))
        hr = apps->Add(app);
      app->Release();
    }
    apps->Release();
  }
  return SUCCEEDED(hr);
}

} // namespace cricket
