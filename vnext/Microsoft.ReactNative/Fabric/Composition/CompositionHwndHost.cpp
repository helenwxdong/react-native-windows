// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "CompositionHwndHost.h"
#include "CompositionHwndHost.g.cpp"

#include <Fabric/Composition/CompositionUIService.h>
#include <QuirkSettings.h>
#include <ReactHost/MsoUtils.h>
#include <Utils/Helpers.h>
#include <dispatchQueue/dispatchQueue.h>
#include <windowsx.h>
#include <winrt/Windows.UI.Core.h>
#include "CompositionContextHelper.h"
#include "ReactNativeHost.h"

WINUSERAPI UINT WINAPI GetDpiForWindow(_In_ HWND hwnd);

namespace winrt::Microsoft::ReactNative::implementation {

winrt::Windows::UI::Composition::Visual CompositionHwndHost::RootVisual() const noexcept {
  return m_target.Root();
}

winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget CompositionHwndHost::Target() const noexcept {
  assert(m_target);
  return m_target;
}

void CompositionHwndHost::CreateDesktopWindowTarget(HWND window) {
  namespace abi = ABI::Windows::UI::Composition::Desktop;

  auto interop = Compositor().as<abi::ICompositorDesktopInterop>();
  winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
  check_hresult(interop->CreateDesktopWindowTarget(
      window, false, reinterpret_cast<abi::IDesktopWindowTarget **>(put_abi(target))));
  m_target = target;
}

void CompositionHwndHost::CreateCompositionRoot() {
  auto root = Compositor().CreateContainerVisual();
  root.RelativeSizeAdjustment({1.0f, 1.0f});
  root.Offset({0, 0, 0});
  m_target.Root(root);
}

CompositionHwndHost::CompositionHwndHost() noexcept {}

void CompositionHwndHost::Initialize(uint64_t hwnd) noexcept {
  m_hwnd = (HWND)hwnd;

  m_compRootView = winrt::Microsoft::ReactNative::CompositionRootView();

  CreateDesktopWindowTarget(m_hwnd);
  CreateCompositionRoot();

  EnsureTarget();

  m_compRootView.InitialProps(std::move(m_initialPropsWriter));
  m_compRootView.ComponentName(std::move(m_componentName));
  m_compRootView.ReactNativeHost(std::move(m_reactNativeHost));

  m_compRootView.ScaleFactor(ScaleFactor());
  m_compRootView.RootVisual(
      winrt::Microsoft::ReactNative::Composition::implementation::CompositionContextHelper::CreateVisual(RootVisual()));

  UpdateSize();
}

double CompositionHwndHost::ScaleFactor() noexcept {
  return GetDpiForWindow(m_hwnd) / 96.0;
}

void CompositionHwndHost::UpdateSize() noexcept {
  RECT rc;
  if (GetClientRect(m_hwnd, &rc)) {
    winrt::Windows::Foundation::Size size{
        static_cast<float>((rc.right - rc.left) / ScaleFactor()),
        static_cast<float>((rc.bottom - rc.top) / ScaleFactor())};
    m_compRootView.Size(size);
    m_compRootView.Measure(size);
    m_compRootView.Arrange(size);
  }
}

LRESULT CompositionHwndHost::TranslateMessage(int msg, uint64_t wParam, int64_t lParam) noexcept {
  if (!m_hwnd || !m_compRootView)
    return 0;

  switch (msg) {
    case WM_MOUSEWHEEL: {
      POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ::ScreenToClient(m_hwnd, &pt);
      int32_t delta = GET_WHEEL_DELTA_WPARAM(wParam);
      m_compRootView.OnScrollWheel({static_cast<float>(pt.x), static_cast<float>(pt.y)}, delta);
      return 0;
    }
    /*
    case WM_POINTERDOWN: {
      m_compRootView.OnPointerPressed({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
      return 0;
    }
    case WM_LBUTTONUP: {
      m_compRootView.OnMouseUp({static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))});
      return 0;
    }
    */
    case WM_WINDOWPOSCHANGED: {
      UpdateSize();
      /*
      winrt::Windows::Foundation::Size size{
          static_cast<float>(reinterpret_cast<const WINDOWPOS *>(lParam)->cx),
          static_cast<float>(reinterpret_cast<const WINDOWPOS *>(lParam)->cy)};
      m_compRootView.Size(size);
      m_compRootView.Measure(size);
      m_compRootView.Arrange(size);
      */
      return 0;
    }
  }

  if (m_compRootView) {
    return static_cast<LRESULT>(m_compRootView.SendMessage(msg, wParam, lParam));
  }
  return 0;
}

ReactNative::ReactNativeHost CompositionHwndHost::ReactNativeHost() const noexcept {
  return m_reactNativeHost ? m_reactNativeHost : m_compRootView.ReactNativeHost();
}

void CompositionHwndHost::ReactNativeHost(ReactNative::ReactNativeHost const &value) noexcept {
  if (m_compRootView) {
    m_compRootView.ReactNativeHost(value);
  } else {
    m_reactNativeHost = value;
  }
  EnsureTarget();
}

winrt::Windows::UI::Composition::Compositor CompositionHwndHost::Compositor() const noexcept {
  auto compositionContext =
      winrt::Microsoft::ReactNative::Composition::implementation::CompositionUIService::GetCompositionContext(
          ReactNativeHost().InstanceSettings().Properties());

  return winrt::Microsoft::ReactNative::Composition::implementation::CompositionContextHelper::InnerCompositor(
      compositionContext);
}

void CompositionHwndHost::EnsureTarget() noexcept {
  if (!ReactNativeHost() || !m_hwnd) {
    return;
  }

  winrt::Microsoft::ReactNative::ReactPropertyBag propBag(ReactNativeHost().InstanceSettings().Properties());
  propBag.Set(
      winrt::Microsoft::ReactNative::ReactPropertyId<winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget>(
          L"CompCoreDispatcher"),
      Target());
}

winrt::hstring CompositionHwndHost::ComponentName() noexcept {
  return m_compRootView ? m_compRootView.ComponentName() : m_componentName;
}

void CompositionHwndHost::ComponentName(winrt::hstring const &value) noexcept {
  if (m_compRootView)
    m_compRootView.ComponentName(value);
  else
    m_componentName = value;
}

ReactNative::JSValueArgWriter CompositionHwndHost::InitialProps() noexcept {
  return m_compRootView ? m_compRootView.InitialProps() : m_initialPropsWriter;
}

void CompositionHwndHost::InitialProps(ReactNative::JSValueArgWriter const &value) noexcept {
  if (m_compRootView)
    m_compRootView.InitialProps(value);
  else
    m_initialPropsWriter = value;
}

} // namespace winrt::Microsoft::ReactNative::implementation
