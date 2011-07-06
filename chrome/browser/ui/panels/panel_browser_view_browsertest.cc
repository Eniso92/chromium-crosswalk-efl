// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_win.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/in_process_browser_test.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/slide_animation.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"

class PanelBrowserViewTest : public InProcessBrowserTest {
 public:
  PanelBrowserViewTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }

 protected:
  struct MenuItem {
    int id;
    bool enabled;
  };

  class MockMouseWatcher : public PanelBrowserFrameView::MouseWatcher {
   public:
    explicit MockMouseWatcher(PanelBrowserFrameView* view)
        : PanelBrowserFrameView::MouseWatcher(view),
          is_cursor_in_view_(false) {
    }

    virtual bool IsCursorInViewBounds() const {
      return is_cursor_in_view_;
    }

    void MoveMouse(bool is_cursor_in_view) {
      is_cursor_in_view_ = is_cursor_in_view;

#if defined(OS_WIN)
      MSG msg;
      msg.message = WM_MOUSEMOVE;
      DidProcessMessage(msg);
#else
      GdkEvent event;
      event.type = GDK_MOTION_NOTIFY;
      DidProcessEvent(&event);
#endif
    }

   private:
    bool is_cursor_in_view_;
  };

  enum ShowFlag { SHOW_AS_ACTIVE, SHOW_AS_INACTIVE };

  PanelBrowserView* CreatePanelBrowserView(const std::string& panel_name,
                                           ShowFlag show_flag) {
    Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                   panel_name,
                                                   gfx::Rect(),
                                                   browser()->profile());
    Panel* panel = static_cast<Panel*>(panel_browser->window());
    if (show_flag == SHOW_AS_ACTIVE)
      panel->Show();
    else
      panel->ShowInactive();
    return static_cast<PanelBrowserView*>(panel->native_panel());
  }

  void ValidateDragging(PanelBrowserView** browser_views,
                        size_t num_browser_views,
                        size_t index_to_drag,
                        int delta_x,
                        int delta_y,
                        int* expected_delta_x_after_drag,
                        int* expected_delta_x_after_release,
                        bool cancel_dragging) {
    // Keep track of the initial bounds for comparison.
    scoped_array<gfx::Rect> initial_bounds(new gfx::Rect[num_browser_views]);
    for (size_t i = 0; i < num_browser_views; ++i)
      initial_bounds[i] = browser_views[i]->panel()->GetRestoredBounds();

    // Trigger the mouse-pressed event.
    // All panels should remain in their original positions.
    views::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                              initial_bounds[index_to_drag].x(),
                              initial_bounds[index_to_drag].y(),
                              ui::EF_LEFT_BUTTON_DOWN);
    browser_views[index_to_drag]->OnTitleBarMousePressed(pressed);
    gfx::Rect bounds_after_press =
        browser_views[index_to_drag]->panel()->GetRestoredBounds();
    for (size_t i = 0; i < num_browser_views; ++i) {
      EXPECT_EQ(initial_bounds[i],
                browser_views[i]->panel()->GetRestoredBounds());
    }

    // Trigger the mouse_dragged event if delta_x or delta_y is not 0.
    views::MouseEvent dragged(ui::ET_MOUSE_DRAGGED,
                              initial_bounds[index_to_drag].x() + delta_x,
                              initial_bounds[index_to_drag].y() + delta_y,
                              ui::EF_LEFT_BUTTON_DOWN);
    if (delta_x || delta_y) {
      browser_views[index_to_drag]->OnTitleBarMouseDragged(dragged);

      for (size_t i = 0; i < num_browser_views; ++i) {
        gfx::Rect expected_bounds = initial_bounds[i];
        expected_bounds.Offset(expected_delta_x_after_drag[i], 0);
        EXPECT_EQ(expected_bounds,
                  browser_views[i]->panel()->GetRestoredBounds());
      }
    }

    // Cancel the dragging if asked.
    // All panels should stay in their original positions.
    if (cancel_dragging) {
      browser_views[index_to_drag]->OnTitleBarMouseCaptureLost();
      for (size_t i = 0; i < num_browser_views; ++i) {
        EXPECT_EQ(initial_bounds[i],
                  browser_views[i]->panel()->GetRestoredBounds());
      }
      return;
    }

    // Otherwise, trigger the mouse-released event.
    views::MouseEvent released(ui::ET_MOUSE_RELEASED, 0, 0, 0);
    browser_views[index_to_drag]->OnTitleBarMouseReleased(released);
    for (size_t i = 0; i < num_browser_views; ++i) {
      gfx::Rect expected_bounds = initial_bounds[i];
       expected_bounds.Offset(expected_delta_x_after_release[i], 0);
      EXPECT_EQ(expected_bounds,
                browser_views[i]->panel()->GetRestoredBounds());
    }

    // If releasing the drag causes the panels to swap, move these panels back
    // so that the next test will not be affected.
    if (initial_bounds[index_to_drag] !=
        browser_views[index_to_drag]->panel()->GetRestoredBounds()) {
      // Find the index of the panel to swap with the dragging panel.
      size_t swapped_index = 0;
      for (; swapped_index < num_browser_views; ++swapped_index) {
        if (initial_bounds[index_to_drag] ==
            browser_views[swapped_index]->panel()->GetRestoredBounds())
          break;
      }
      ASSERT_NE(swapped_index, index_to_drag);

      // Repeat the mouse events to this panel.
      browser_views[swapped_index]->OnTitleBarMousePressed(pressed);
      browser_views[swapped_index]->OnTitleBarMouseDragged(dragged);
      browser_views[swapped_index]->OnTitleBarMouseReleased(released);
      for (size_t i = 0; i < num_browser_views; ++i) {
        EXPECT_EQ(initial_bounds[i],
                  browser_views[i]->panel()->GetRestoredBounds());
      }
    }
  }

  void ValidateSettingsMenuItems(ui::SimpleMenuModel* settings_menu_contents,
                                 size_t num_expected_menu_items,
                                 const MenuItem* expected_menu_items) {
    ASSERT_TRUE(settings_menu_contents);
    EXPECT_EQ(static_cast<int>(num_expected_menu_items),
              settings_menu_contents->GetItemCount());
    for (size_t i = 0; i < num_expected_menu_items; ++i) {
      if (expected_menu_items[i].id == -1) {
        EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
                  settings_menu_contents->GetTypeAt(i));
      } else {
        EXPECT_EQ(expected_menu_items[i].id,
                  settings_menu_contents->GetCommandIdAt(i));
        EXPECT_EQ(expected_menu_items[i].enabled,
                  settings_menu_contents->IsEnabledAt(i));
      }
    }
  }

  void TestCreateSettingsMenuForExtension(const FilePath::StringType& path,
                                          Extension::Location location,
                                          const std::string& homepage_url,
                                          const std::string& options_page) {
    // Creates a testing extension.
#if defined(OS_WIN)
    FilePath full_path(FILE_PATH_LITERAL("c:\\"));
#else
    FilePath full_path(FILE_PATH_LITERAL("/"));
#endif
    full_path = full_path.Append(path);
    DictionaryValue input_value;
    input_value.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
    input_value.SetString(extension_manifest_keys::kName, "Sample Extension");
    if (!homepage_url.empty()) {
      input_value.SetString(extension_manifest_keys::kHomepageURL,
                            homepage_url);
    }
    if (!options_page.empty()) {
      input_value.SetString(extension_manifest_keys::kOptionsPage,
                            options_page);
    }
    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        full_path, location, input_value, Extension::STRICT_ERROR_CHECKS,
        &error);
    ASSERT_TRUE(extension.get());
    EXPECT_STREQ("", error.c_str());
    browser()->GetProfile()->GetExtensionService()->OnLoadSingleExtension(
        extension.get());

    // Creates a panel with the app name that comes from the extension ID.
    PanelBrowserView* browser_view = CreatePanelBrowserView(
        web_app::GenerateApplicationNameFromExtensionId(extension->id()),
        SHOW_AS_ACTIVE);
    PanelBrowserFrameView* frame_view = browser_view->GetFrameView();

    frame_view->EnsureSettingsMenuCreated();

    // Validates the settings menu items.
    MenuItem expected_panel_menu_items[] = {
        { PanelBrowserFrameView::COMMAND_NAME, false },
        { -1, false },  // Separator
        { PanelBrowserFrameView::COMMAND_CONFIGURE, false },
        { PanelBrowserFrameView::COMMAND_DISABLE, false },
        { PanelBrowserFrameView::COMMAND_UNINSTALL, false },
        { -1, false },  // Separator
        { PanelBrowserFrameView::COMMAND_MANAGE, true }
    };
    if (!homepage_url.empty())
      expected_panel_menu_items[0].enabled = true;
    if (!options_page.empty())
      expected_panel_menu_items[2].enabled = true;
    if (location != Extension::EXTERNAL_POLICY_DOWNLOAD) {
      expected_panel_menu_items[3].enabled = true;
      expected_panel_menu_items[4].enabled = true;
    }
    ValidateSettingsMenuItems(&frame_view->settings_menu_contents_,
                              arraysize(expected_panel_menu_items),
                              expected_panel_menu_items);

    browser_view->panel()->Close();
  }

  void TestShowPanelActiveOrInactive() {
    PanelBrowserView* browser_view1 = CreatePanelBrowserView("PanelTest1",
                                                             SHOW_AS_ACTIVE);
    PanelBrowserFrameView* frame_view1 = browser_view1->GetFrameView();
    EXPECT_TRUE(browser_view1->panel()->IsActive());
    EXPECT_EQ(PanelBrowserFrameView::PAINT_AS_ACTIVE,
              frame_view1->paint_state_);

    PanelBrowserView* browser_view2 = CreatePanelBrowserView("PanelTest2",
                                                             SHOW_AS_INACTIVE);
    PanelBrowserFrameView* frame_view2 = browser_view2->GetFrameView();
    EXPECT_FALSE(browser_view2->panel()->IsActive());
    EXPECT_EQ(PanelBrowserFrameView::PAINT_AS_INACTIVE,
              frame_view2->paint_state_);

    browser_view1->panel()->Close();
    browser_view2->panel()->Close();
  }

  // We put all the testing logic in this class instead of the test so that
  // we do not need to declare each new test as a friend of PanelBrowserView
  // for the purpose of accessing its private members.
  void TestMinimizeAndRestore() {
    PanelBrowserView* browser_view1 = CreatePanelBrowserView("PanelTest1",
                                                             SHOW_AS_ACTIVE);
    Panel* panel1 = browser_view1->panel_.get();

    // Test minimizing/restoring an individual panel.
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    int initial_height = panel1->GetBounds().height();
    int titlebar_height =
        browser_view1->GetFrameView()->NonClientTopBorderHeight();

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);
    EXPECT_TRUE(IsMouseWatcherStarted());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());

    panel1->SetExpansionState(Panel::EXPANDED);
    EXPECT_EQ(Panel::EXPANDED, panel1->expansion_state());
    EXPECT_EQ(initial_height, panel1->GetBounds().height());

    panel1->SetExpansionState(Panel::TITLE_ONLY);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(titlebar_height, panel1->GetBounds().height());

    panel1->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_LT(panel1->GetBounds().height(), titlebar_height);
    EXPECT_GT(panel1->GetBounds().height(), 0);

    // Create 2 more panels for more testing.
    PanelBrowserView* browser_view2 = CreatePanelBrowserView("PanelTest2",
                                                             SHOW_AS_ACTIVE);
    Panel* panel2 = browser_view2->panel_.get();

    PanelBrowserView* browser_view3 = CreatePanelBrowserView("PanelTest3",
                                                             SHOW_AS_ACTIVE);
    Panel* panel3 = browser_view3->panel_.get();

    // Test bringing up or down the title-bar of all minimized panels.
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    panel3->SetExpansionState(Panel::MINIMIZED);
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());

    PanelManager* panel_manager = PanelManager::GetInstance();

    panel_manager->BringUpOrDownTitleBarForAllMinimizedPanels(true);
    EXPECT_EQ(Panel::TITLE_ONLY, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::TITLE_ONLY, panel3->expansion_state());

    panel_manager->BringUpOrDownTitleBarForAllMinimizedPanels(false);
    EXPECT_EQ(Panel::MINIMIZED, panel1->expansion_state());
    EXPECT_EQ(Panel::EXPANDED, panel2->expansion_state());
    EXPECT_EQ(Panel::MINIMIZED, panel3->expansion_state());

    // Test if it is OK to bring up title-bar given the mouse position.
    EXPECT_TRUE(panel_manager->ShouldBringUpTitleBarForAllMinimizedPanels(
        panel1->GetBounds().x(), panel1->GetBounds().y()));
    EXPECT_FALSE(panel_manager->ShouldBringUpTitleBarForAllMinimizedPanels(
        panel2->GetBounds().x(), panel2->GetBounds().y()));
    EXPECT_TRUE(panel_manager->ShouldBringUpTitleBarForAllMinimizedPanels(
        panel3->GetBounds().right() - 1, panel3->GetBounds().bottom() - 1));
    EXPECT_TRUE(panel_manager->ShouldBringUpTitleBarForAllMinimizedPanels(
        panel3->GetBounds().right() - 1, panel3->GetBounds().bottom() + 10));
    EXPECT_FALSE(panel_manager->ShouldBringUpTitleBarForAllMinimizedPanels(
        0, 0));

    panel1->Close();
    EXPECT_TRUE(IsMouseWatcherStarted());
    panel2->Close();
    EXPECT_TRUE(IsMouseWatcherStarted());
    panel3->Close();
    EXPECT_FALSE(IsMouseWatcherStarted());
  }
};

// Panel is not supported for Linux view yet.
#if !defined(OS_LINUX) || !defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreatePanel) {
  PanelBrowserView* browser_view = CreatePanelBrowserView("PanelTest",
                                                          SHOW_AS_ACTIVE);
  PanelBrowserFrameView* frame_view = browser_view->GetFrameView();

  // The bounds animation should not be triggered when the panel is up for the
  // first time.
  EXPECT_FALSE(browser_view->bounds_animator_.get());

  // We should have icon, text, settings button and close button.
  EXPECT_EQ(4, frame_view->child_count());
  EXPECT_TRUE(frame_view->Contains(frame_view->title_icon_));
  EXPECT_TRUE(frame_view->Contains(frame_view->title_label_));
  EXPECT_TRUE(frame_view->Contains(frame_view->settings_button_));
  EXPECT_TRUE(frame_view->Contains(frame_view->close_button_));

  // These controls should be visible.
  EXPECT_TRUE(frame_view->title_icon_->IsVisible());
  EXPECT_TRUE(frame_view->title_label_->IsVisible());
  EXPECT_TRUE(frame_view->close_button_->IsVisible());

  // Validate their layouts.
  int title_bar_height = frame_view->NonClientTopBorderHeight() -
      frame_view->NonClientBorderThickness();
  EXPECT_GT(frame_view->title_icon_->width(), 0);
  EXPECT_GT(frame_view->title_icon_->height(), 0);
  EXPECT_LT(frame_view->title_icon_->height(), title_bar_height);
  EXPECT_GT(frame_view->title_label_->width(), 0);
  EXPECT_GT(frame_view->title_label_->height(), 0);
  EXPECT_LT(frame_view->title_label_->height(), title_bar_height);
  EXPECT_GT(frame_view->settings_button_->width(), 0);
  EXPECT_GT(frame_view->settings_button_->height(), 0);
  EXPECT_LT(frame_view->settings_button_->height(), title_bar_height);
  EXPECT_GT(frame_view->close_button_->width(), 0);
  EXPECT_GT(frame_view->close_button_->height(), 0);
  EXPECT_LT(frame_view->close_button_->height(), title_bar_height);
  EXPECT_LT(frame_view->title_icon_->x() + frame_view->title_icon_->width(),
      frame_view->title_label_->x());
  EXPECT_LT(frame_view->title_label_->x() + frame_view->title_label_->width(),
      frame_view->settings_button_->x());
  EXPECT_LT(
      frame_view->settings_button_->x() + frame_view->settings_button_->width(),
      frame_view->close_button_->x());

  // Validate that the controls should be updated when the activation state is
  // changed.
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_ACTIVE);
  gfx::Font title_label_font1 = frame_view->title_label_->font();
  frame_view->UpdateControlStyles(PanelBrowserFrameView::PAINT_AS_INACTIVE);
  gfx::Font title_label_font2 = frame_view->title_label_->font();
  EXPECT_NE(title_label_font1.GetStyle(), title_label_font2.GetStyle());

  browser_view->panel()->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, ShowPanelActiveOrInactive) {
  TestShowPanelActiveOrInactive();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, ShowOrHideSettingsButton) {
  PanelBrowserView* browser_view = CreatePanelBrowserView("PanelTest",
                                                          SHOW_AS_ACTIVE);
  PanelBrowserFrameView* frame_view = browser_view->GetFrameView();

  // Create and hook up the MockMouseWatcher so that we can simulate if the
  // mouse is over the panel.
  MockMouseWatcher* mouse_watcher = new MockMouseWatcher(frame_view);
  frame_view->set_mouse_watcher(mouse_watcher);

  // When the panel is created, it is active. Since we cannot programatically
  // bring the panel back to active state once it is deactivated, we have to
  // test the cases that the panel is active first.
  EXPECT_TRUE(browser_view->panel()->IsActive());

  // When the panel is active, the settings button should always be visible.
  mouse_watcher->MoveMouse(true);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());
  mouse_watcher->MoveMouse(false);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());

  // When the panel is inactive, the options button is active per the mouse over
  // the panel or not.
  browser_view->panel()->Deactivate();
  EXPECT_FALSE(browser_view->panel()->IsActive());

  mouse_watcher->MoveMouse(true);
  EXPECT_TRUE(frame_view->settings_button_->IsVisible());
  mouse_watcher->MoveMouse(false);
  EXPECT_FALSE(frame_view->settings_button_->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, TitleBarMouseEvent) {
  // TODO(jianli): Move the test to platform-independent PanelManager unittest.

  // Creates the 1st panel.
  PanelBrowserView* browser_view1 = CreatePanelBrowserView("PanelTest1",
                                                           SHOW_AS_ACTIVE);

  // The delta is from the pressed location which is the left-top corner of the
  // panel to drag.
  int small_negative_delta = -5;
  int small_positive_delta = 5;
  int big_negative_delta =
      -(browser_view1->panel()->GetRestoredBounds().width() * 0.5 + 5);
  int big_positive_delta =
      browser_view1->panel()->GetRestoredBounds().width() * 1.5 + 5;

  // Tests dragging a panel in single-panel environment.
  // We should get back to the original position after the dragging is ended.
  int expected_single_delta_x_after_drag[] = { big_negative_delta };
  int expected_single_delta_x_after_release[] = { 0 };
  ValidateDragging(&browser_view1,
                   1,
                   0,
                   big_negative_delta,
                   big_negative_delta,
                   expected_single_delta_x_after_drag,
                   expected_single_delta_x_after_release,
                   false);

  expected_single_delta_x_after_drag[0] = big_positive_delta;
  ValidateDragging(&browser_view1,
                   1,
                   0,
                   big_positive_delta,
                   big_positive_delta,
                   expected_single_delta_x_after_drag,
                   expected_single_delta_x_after_release,
                   false);

  // Creates 2 more panels to test dragging a panel in multi-panel environment.
  PanelBrowserView* browser_view2 = CreatePanelBrowserView("PanelTest2",
                                                           SHOW_AS_ACTIVE);
  PanelBrowserView* browser_view3 = CreatePanelBrowserView("PanelTest3",
                                                           SHOW_AS_ACTIVE);
  PanelBrowserView* browser_views[] =
    { browser_view1, browser_view2, browser_view3 };
  size_t browser_view_count = arraysize(browser_views);
  int expected_delta_x_without_change[] = { 0, 0, 0 };

  // Tests dragging a panel with small delta.
  // We should get back to the original position after the dragging is ended.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };

    expected_delta_x_after_drag[i] = small_negative_delta;
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     small_negative_delta,
                     small_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     false);

    expected_delta_x_after_drag[i] = small_positive_delta;
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     small_positive_delta,
                     small_positive_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     false);
  }

  // Tests dragging a panel with big delta and then cancelling it.
  // We should get back to the original position.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };
    expected_delta_x_after_drag[i] = big_negative_delta;
    if (i + 1 < browser_view_count) {
      expected_delta_x_after_drag[i + 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i + 1]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_negative_delta,
                     big_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_without_change,
                     true);

    int expected_delta_x_after_drag2[] = { 0, 0, 0 };
    expected_delta_x_after_drag2[i] = big_positive_delta;
    if (static_cast<int>(i) - 1 >= 0) {
      expected_delta_x_after_drag2[i - 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i - 1]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_positive_delta,
                     big_positive_delta,
                     expected_delta_x_after_drag2,
                     expected_delta_x_without_change,
                     true);
  }

  // Tests dragging a panel with big delta.
  // We should move to the new position after the dragging is ended.
  for (size_t i = 0; i < browser_view_count; ++i) {
    int expected_delta_x_after_drag[] = { 0, 0, 0 };
    int expected_delta_x_after_release[] = { 0, 0, 0 };
    expected_delta_x_after_drag[i] = big_negative_delta;
    if (i + 1 < browser_view_count) {
      expected_delta_x_after_drag[i + 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i + 1]->panel()->GetRestoredBounds().x();
      expected_delta_x_after_release[i + 1] =
          expected_delta_x_after_drag[i + 1];
      expected_delta_x_after_release[i] =
          browser_views[i + 1]->panel()->GetRestoredBounds().x() -
          browser_views[i]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_negative_delta,
                     big_negative_delta,
                     expected_delta_x_after_drag,
                     expected_delta_x_after_release,
                     false);

    int expected_delta_x_after_drag2[] = { 0, 0, 0 };
    int expected_delta_x_after_release2[] = { 0, 0, 0 };
    expected_delta_x_after_drag2[i] = big_positive_delta;
    if (static_cast<int>(i) - 1 >= 0) {
      expected_delta_x_after_drag2[i - 1] =
          browser_views[i]->panel()->GetRestoredBounds().x() -
          browser_views[i - 1]->panel()->GetRestoredBounds().x();
      expected_delta_x_after_release2[i - 1] =
          expected_delta_x_after_drag2[i - 1];
      expected_delta_x_after_release2[i] =
          browser_views[i - 1]->panel()->GetRestoredBounds().x() -
          browser_views[i]->panel()->GetRestoredBounds().x();
    }
    ValidateDragging(browser_views,
                     browser_view_count,
                     i,
                     big_positive_delta,
                     big_positive_delta,
                     expected_delta_x_after_drag2,
                     expected_delta_x_after_release2,
                     false);
  }

  // Closes all panels.
  for (size_t i = 0; i < browser_view_count; ++i) {
    browser_views[i]->panel()->Close();
    // We're only verifying that PanelBrowserView::Close has been called when
    // a panel is closed. This is to make sure the dragging is ended and does
    // not interfere the closing process.
    EXPECT_TRUE(browser_views[i]->closed());
  }
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, SetBoundsAnimation) {
  PanelBrowserView* browser_view = CreatePanelBrowserView("PanelTest",
                                                          SHOW_AS_ACTIVE);

  // Validate that animation should be triggered when SetBounds is called.
  gfx::Rect target_bounds(browser_view->GetBounds());
  target_bounds.Offset(20, -30);
  target_bounds.set_width(target_bounds.width() + 100);
  target_bounds.set_height(target_bounds.height() + 50);
  browser_view->SetBounds(target_bounds);
  ASSERT_TRUE(browser_view->bounds_animator_.get());
  EXPECT_TRUE(browser_view->bounds_animator_->is_animating());
  EXPECT_NE(browser_view->GetBounds(), target_bounds);
  // The timer for the animation will only kick in as async task.
  while (browser_view->bounds_animator_->is_animating()) {
    MessageLoop::current()->RunAllPending();
  }
  EXPECT_EQ(browser_view->GetBounds(), target_bounds);

  // Validates that no animation should be triggered for the panel currently
  // being dragged.
  views::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                            target_bounds.x(),
                            target_bounds.y(),
                            ui::EF_LEFT_BUTTON_DOWN);
  browser_view->OnTitleBarMousePressed(pressed);

  views::MouseEvent dragged(ui::ET_MOUSE_DRAGGED,
                            target_bounds.x() + 5,
                            target_bounds.y() + 5,
                            ui::EF_LEFT_BUTTON_DOWN);
  browser_view->OnTitleBarMouseDragged(dragged);
  EXPECT_FALSE(browser_view->bounds_animator_->is_animating());
  browser_view->OnTitleBarMouseCaptureLost();

  browser_view->panel()->Close();
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, CreateSettingsMenu) {
  TestCreateSettingsMenuForExtension(
      FILE_PATH_LITERAL("extension1"), Extension::EXTERNAL_POLICY_DOWNLOAD,
      "", "");
  TestCreateSettingsMenuForExtension(
      FILE_PATH_LITERAL("extension2"), Extension::INVALID,
      "http://home", "options.html");
}

IN_PROC_BROWSER_TEST_F(PanelBrowserViewTest, MinimizeAndRestore) {
  TestMinimizeAndRestore();
}
#endif
