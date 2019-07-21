﻿//
// Project: CParser
// Created by bajdcc
//

#include "stdafx.h"
#include "cvm.h"
#include "cwnd.h"

#define WINDOW_BORDER_X 5
#define WINDOW_TITLE_Y 30
#define WINDOW_HANG_MS 100ms
#define WINDOW_HANG_BLUR 5
#define WINDOW_CLOSE_BTN_X 20
#define WINDOW_MIN_SIZE_X 100
#define WINDOW_MIN_SIZE_Y 40

namespace clib {

    vfs_node_stream_window::vfs_node_stream_window(const vfs_mod_query* mod, vfs_stream_t s, vfs_stream_call* call, int id) :
        vfs_node_dec(mod), call(call) {
        wnd = call->stream_getwnd(id);
    }

    vfs_node_stream_window::~vfs_node_stream_window()
    {
    }

    vfs_node_dec* vfs_node_stream_window::create(const vfs_mod_query* mod, vfs_stream_t s, vfs_stream_call* call, const string_t& path)
    {
        static string_t pat{ R"(/handle/([0-9]+)/message)" };
        static std::regex re(pat);
        std::smatch res;
        if (std::regex_match(path, res, re)) {
            auto id = atoi(res[1].str().c_str());
            return new vfs_node_stream_window(mod, s, call, id);
        }
        return nullptr;
    }

    bool vfs_node_stream_window::available() const {
        return wnd->get_state() != cwindow::W_CLOSING;
    }

    int vfs_node_stream_window::index() const {
        if (!available())return READ_EOF;
        auto d = wnd->get_msg_data();
        if (d == -1)
            return DELAY_CHAR;
        return (int)d;
    }

    void vfs_node_stream_window::advance() {

    }

    int vfs_node_stream_window::write(byte c) {
        return -1;
    }

    int vfs_node_stream_window::truncate() {
        return -1;
    }

    int sys_cursor(int cx, int cy) {
        using CT = Window::CursorType;
        static int curs[] = {
            CT::size_topleft, CT::size_left, CT::size_topright,
            CT::size_top, CT::arrow, CT::size_top,
            CT::size_topright, CT::size_left, CT::size_topleft,
        };
        return curs[(cx + 1) * 3 + (cy + 1)];
    }

    CColor cwindow_style_win::get_color(color_t t) const
    {
        if (t > c__none && t < c__end) {
            switch (t) {
            case c_window_nonclient:
                return CColor(45, 120, 213);
            case c_window_background:
                return CColor(238, 238, 238);
            case c_window_title_text:
                return CColor(Gdiplus::Color::White);
            case c_button_bg_def:
                return CColor(204, 204, 204);
            case c_button_bg_focus:
                return CColor(Gdiplus::Color::White);
            case c_button_fg_def:
                return CColor(51, 51, 51);
            case c_button_fg_hover:
                return CColor(Gdiplus::Color::Black);
            }
        }
        ATLVERIFY(!"undefined style color!");
        return CColor();
    }

    string_t cwindow_style_win::get_str(str_t t) const
    {
        if (t > s__none && t < s__end) {
            switch (t) {
            case s_title_hang:
                return "(未响应)";
            }
        }
        ATLVERIFY(!"undefined style str!");
        return "";
    }

    int cwindow_style_win::get_int(px_t t) const
    {
        if (t > p__none && t < p__end) {
            switch (t) {
            case p_title_y:
                return WINDOW_TITLE_Y;
            case p_close_btn_x:
                return WINDOW_CLOSE_BTN_X;
            case p_border_x:
                return WINDOW_BORDER_X;
            case p_hang_blur:
                return WINDOW_HANG_BLUR;
            case p_title_tl_x:
                return 5;
            case p_title_tl_y:
                return 2;
            case p_min_size_x:
                return WINDOW_MIN_SIZE_X;
            case p_min_size_y:
                return WINDOW_MIN_SIZE_Y;
            }
        }
        ATLVERIFY(!"undefined style int!");
        return 0;
    }

    float cwindow_style_win::get_float(float_t t) const
    {
        if (t > f__none && t < f__end) {
            switch (t) {
            case f_button_radius:
                return 5.0f;
            }
        }
        ATLVERIFY(!"undefined style int!");
        return 0.0f;
    }

    cwindow::cwindow(cvm* vm, int handle, const string_t& caption, const CRect& location)
        : vm(vm), handle(handle), caption(caption), location(location), self_min_size(WINDOW_MIN_SIZE_X, WINDOW_MIN_SIZE_Y)
    {
        style = std::make_shared<cwindow_style_win>();
        auto bg = SolidBackgroundElement::Create();
        bg->SetColor(style->get_color(cwindow_style::c_window_background));
        root = bg;
        root->SetRenderRect(location);
        renderTarget = std::make_shared<Direct2DRenderTarget>(window->shared_from_this());
        renderTarget->Init();
        _init();
        root->GetRenderer()->SetRenderTarget(renderTarget);
    }

    cwindow::~cwindow()
    {
        auto& focus = cvm::global_state.window_focus;
        auto& hover = cvm::global_state.window_hover;
        if (focus == handle)
            focus = -1;
        if (hover == handle)
            hover = -1;
        destroy();
    }

    void cwindow::init()
    {
        const auto& s = location;
        hit(200, s.left + 1, s.top + 1);
        //hit(211, s.left + 1, s.top + 1);
        hit(201, s.left + 1, s.top + 1);
        time_handler = std::chrono::system_clock::now();
    }

    void cwindow::paint(const CRect& bounds)
    {
        using namespace std::chrono_literals;
        if (state == W_RUNNING) {
            if (msg_data.empty()) {
                state = W_IDLE;
            }
            else if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - time_handler) >= WINDOW_HANG_MS) {
                bag.title_text->SetText(CString(CStringA((caption +
                    style->get_str(cwindow_style::s_title_hang)).c_str())));
                state = W_BUSY;
            }
        }
        else if (state == W_IDLE) {
            if (!msg_data.empty()) {
                state = W_RUNNING;
            }
        }
        else if (state == W_BUSY) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - time_handler) < WINDOW_HANG_MS) {
                bag.title_text->SetText(CString(CStringA(caption.c_str())));
                state = W_RUNNING;
            }
        }
        if (bounds1 != bounds || need_repaint) {
            need_repaint = false;
            CRect rt;
            auto border_x = style->get_int(cwindow_style::p_border_x);
            auto title_y = style->get_int(cwindow_style::p_title_y);
            {
                auto mins = bag.comctl->min_size();
                location.right = location.left + __max(location.Width(), mins.cx + border_x * 2);
                location.bottom = location.top + __max(location.Height(), mins.cy + border_x + title_y);
            }
            root->SetRenderRect(location.OfRect(bounds));
            rt = bag.title->GetRenderRect();
            rt.right = root->GetRenderRect().Width();
            rt.bottom = root->GetRenderRect().Height();
            bag.title->SetRenderRect(rt);
            self_title = CRect(0, 0, rt.Width(), title_y);
            auto close_btn_x = style->get_int(cwindow_style::p_close_btn_x);
            if (bag.title_text->GetRenderer()->GetMinSize().cx + close_btn_x * 2 > rt.Width())
                rt.right -= close_btn_x;
            rt.bottom = self_title.Height();
            bag.title_text->SetRenderRect(rt);
            rt.left = border_x;
            rt.top = title_y;
            rt.right = root->GetRenderRect().Width() - border_x;
            rt.bottom = root->GetRenderRect().Height() - border_x;
            bag.client->SetRenderRect(rt);
            rt = bag.close_text->GetRenderRect();
            rt.left = root->GetRenderRect().Width() - close_btn_x;
            rt.right = root->GetRenderRect().Width();
            bag.close_text->SetRenderRect(rt);
            bounds1 = bounds;
            rt.left = rt.top = 0;
            rt.right = bounds.Width();
            rt.bottom = bounds.Height();
            bag.border->SetRenderRect(rt);
            bounds3 = root->GetRenderRect();
            bounds3.top += self_title.Height();
        }
        root->GetRenderer()->Render(root->GetRenderRect());
        if (state == W_BUSY) {
            const int blur_size = style->get_int(cwindow_style::p_hang_blur);;
            auto sz = bounds3.Size();
            sz.cx += blur_size * 2;
            sz.cy += blur_size * 2;
            auto b = renderTarget->CreateBitmapRenderTarget(D2D1::SizeF((float)sz.cx, (float)sz.cy));
            CComPtr<ID2D1Bitmap> bitmap;
            b->GetBitmap(&bitmap);
            auto old = renderTarget->SetDirect2DRenderTarget(b.p);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - time_handler).count();
            auto delay = ((float)ms) * 0.001f;
            delay = __min(delay, blur_size); // (0, 5.0]
            auto clr = byte(255.0f - delay * 20.0f);
            b->BeginDraw();
            b->Clear(GetD2DColor(CColor(clr, clr, clr)));
            bag.comctl->paint(CRect(CPoint(blur_size, blur_size), sz));
            b->EndDraw();
            CComPtr<ID2D1Effect> gaussianBlurEffect;
            auto dev = Direct2D::Singleton().GetDirect2DDeviceContext();
            dev->CreateEffect(CLSID_D2D1GaussianBlur, &gaussianBlurEffect);
            gaussianBlurEffect->SetInput(0, bitmap);
            gaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, delay);
            auto border_x = style->get_int(cwindow_style::p_border_x);
            dev->DrawImage(
                gaussianBlurEffect.p,
                D2D1::Point2F((FLOAT)root->GetRenderRect().left + border_x,
                (FLOAT)root->GetRenderRect().top + self_title.Height()),
                D2D1::RectF((FLOAT)border_x + blur_size, (FLOAT)blur_size,
                (FLOAT)sz.cx - border_x - blur_size, (FLOAT)sz.cy - border_x - blur_size),
                D2D1_INTERPOLATION_MODE_LINEAR
            );
            renderTarget->SetDirect2DRenderTarget(old);
        }
        else {
            bag.comctl->paint(bounds3);
        }
    }

#define WM_MOUSEENTER 0x2A5
    bool cwindow::hit(int n, int x, int y)
    {
        static int mapnc[] = {
            WM_NCLBUTTONDOWN, WM_NCLBUTTONUP, WM_NCLBUTTONDBLCLK,
            WM_NCRBUTTONDOWN, WM_NCRBUTTONUP, WM_NCRBUTTONDBLCLK,
            WM_NCMBUTTONDOWN, WM_NCMBUTTONUP, WM_NCMBUTTONDBLCLK,
            WM_SETFOCUS, WM_KILLFOCUS,
            WM_NCMOUSEMOVE, 0x2A4, WM_NCMOUSELEAVE, WM_NCMOUSEHOVER
        };
        static int mapc[] = {
            WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK,
            WM_RBUTTONDOWN, WM_RBUTTONUP, WM_RBUTTONDBLCLK,
            WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MBUTTONDBLCLK,
            WM_SETFOCUS, WM_KILLFOCUS,
            WM_MOUSEMOVE, WM_MOUSEENTER, WM_MOUSELEAVE, WM_MOUSEHOVER
        };
        self_client = CPoint(x, y);
        auto pt = self_client + -root->GetRenderRect().TopLeft();
        if (self_drag && n == 211 && cvm::global_state.window_hover == handle) {
            post_data(WM_MOVING, x, y); // 发送本窗口MOVNG
            return true;
        }
        if (self_size && n == 211 && cvm::global_state.window_hover == handle) {
            post_data(WM_NCCALCSIZE, self_min_size.cx, self_min_size.cy);
            post_data(WM_SIZING, x, y); // 发送本窗口MOVNG
            return true;
        }
        if (pt.x < 0 || pt.y < 0 || pt.x >= location.Width() || pt.y >= location.Height())
        {
            if (!(self_size || self_drag))
                return false;
        }
        if (n == 200) {
            if (bag.close_text->GetRenderRect().PtInRect(pt)) {
                post_data(WM_CLOSE);
                return true;
            }
            int cx, cy;
            self_size = is_border(pt, cx, cy);
            if (self_size) {
                post_data(WM_MOVE, x, y);
                post_data(WM_SIZE, cx, cy);
            }
        }
        int code = -1;
        if (n >= 200)
            n -= 200;
        else
            n += 3;
        if (n <= 11 && is_nonclient(pt)) {
            code = mapnc[n];
            if (n == 0) {
                post_data(WM_MOVE, x, y); // 发送本窗口MOVE
            }
        }
        else {
            pt.y -= self_title.Height();
            code = mapc[n];
        }
        post_data(code, pt.x, pt.y);
        if (n == 11) {
            //post_data(WM_SETCURSOR, pt.x, pt.y);
            auto& hover = cvm::global_state.window_hover;
            if (hover != -1) { // 当前有鼠标悬停
                if (hover != handle) { // 非当前窗口
                    vm->post_data(hover, WM_MOUSELEAVE); // 给它发送MOUSELEAVE
                    hover = handle; // 置为当前窗口
                    post_data(WM_MOUSEENTER); // 发送本窗口MOUSEENTER
                }
                else if (self_drag) {
                    post_data(WM_MOVING, x, y); // 发送本窗口MOVNG
                }
            }
            else { // 当前没有鼠标悬停
                hover = handle;
                post_data(WM_MOUSEENTER);
            }
        }
        else if (n == 0) {
            auto& focus = cvm::global_state.window_focus;
            if (focus != -1) { // 当前有焦点
                if (focus != handle) { // 非当前窗口
                    vm->post_data(focus, WM_KILLFOCUS); // 给它发送KILLFOCUS
                    focus = handle; // 置为当前窗口
                    vm->post_data(focus, WM_SETFOCUS); // 发送本窗口SETFOCUS
                }
            }
            else { // 当前没有焦点
                focus = handle;
                vm->post_data(focus, WM_SETFOCUS);
            }
        }
        return true;
    }

    cwindow::window_state_t cwindow::get_state() const
    {
        return state;
    }

    int cwindow::get_msg_data()
    {
        if (msg_data.empty())
            return -1;
        auto d = msg_data.front();
        msg_data.pop();
        return d;
    }

    int cwindow::get_cursor() const
    {
        if (state == W_BUSY)
            return Window::wait;
        return cursor;
    }

    int cwindow::handle_msg(const window_msg& msg)
    {
        time_handler = std::chrono::system_clock::now();
        if (msg.comctl != -1) {
            if (valid_handle(msg.comctl)) {
                return handles[msg.comctl].comctl->handle_msg(msg.code, msg.param1, msg.param2);
            }
            return -1;
        }
        switch (msg.code)
        {
        case WM_CLOSE:
            state = W_CLOSING;
            break;
        case WM_SETTEXT:
            set_caption(vm->vmm_getstr(msg.param1));
            break;
        case WM_GETTEXT:
            vm->vmm_setstr(msg.param1, caption);
            break;
        case WM_MOVE:
            self_drag_pt.x = msg.param1;
            self_drag_pt.y = msg.param2;
            self_drag_rt = location;
            break;
        case WM_MOVING:
            location = self_drag_rt;
            location.OffsetRect((LONG)msg.param1 - self_drag_pt.x, (LONG)msg.param2 - self_drag_pt.y);
            need_repaint = true;
            break;
        case WM_SIZE:
            self_size_pt.x = (LONG)msg.param1;
            self_size_pt.y = (LONG)msg.param2;
            break;
        case WM_SIZING:
            location = self_drag_rt;
            if (self_size_pt.x == 1) {
                location.right = __max(location.left + self_min_size.cx, location.right + (LONG)msg.param1 - self_drag_pt.x);
            }
            if (self_size_pt.x == -1) {
                location.left = __min(location.right - self_min_size.cx, location.left + (LONG)msg.param1 - self_drag_pt.x);
            }
            if (self_size_pt.y == 1) {
                location.bottom = __max(location.top + self_min_size.cy, location.bottom + (LONG)msg.param2 - self_drag_pt.y);
            }
            if (self_size_pt.y == -1) {
                location.top = __min(location.bottom - self_min_size.cy, location.top + (LONG)msg.param2 - self_drag_pt.y);
            }
            need_repaint = true;
            break;
        case WM_SETCURSOR:
        {
        }
            break;
        case WM_NCCALCSIZE:
        {
            auto sz = bag.comctl->min_size();
            self_min_size.cx = __max(sz.cx, __max(style->get_int(cwindow_style::p_min_size_x), (LONG)msg.param1));
            self_min_size.cy = __max(sz.cy, __max(style->get_int(cwindow_style::p_min_size_y), (LONG)msg.param2));
        }
            break;
        default:
            break;
        }
        return 0;
    }

    void cwindow::_init()
    {
        auto r = root->GetRenderRect();
        auto& list = root->GetChildren();
        auto title = SolidBackgroundElement::Create();
        title->SetColor(style->get_color(cwindow_style::c_window_nonclient));
        title->SetRenderRect(CRect(0, 0, r.Width(), r.Height()));
        auto title_y = style->get_int(cwindow_style::p_title_y);
        self_title = CRect(0, 0, r.Width(), title_y);
        bag.title = title;
        list.push_back(title);
        root->GetRenderer()->SetRelativePosition(true);
        auto title_text = SolidLabelElement::Create();
        title_text->SetColor(style->get_color(cwindow_style::c_window_title_text));
        title_text->SetText(CString(CStringA(caption.c_str())));
        title_text->SetAlignments(Alignment::StringAlignmentCenter, Alignment::StringAlignmentCenter);
        auto close_btn_x = style->get_int(cwindow_style::p_close_btn_x);
        title_text->SetRenderRect(CRect(style->get_int(cwindow_style::p_title_tl_x),
            style->get_int(cwindow_style::p_title_tl_y),
            r.Width() - close_btn_x, title_y));
        if (title_text->GetRenderer()->GetMinSize().cx + close_btn_x * 2 > r.Width()) {
            auto rr = title_text->GetRenderRect();
            rr.right -= close_btn_x;
            title_text->SetRenderRect(rr);
        }
        bag.title_text = title_text;
        Font f;
        f.size = 20;
        f.fontFamily = "微软雅黑";
        title_text->SetFont(f);
        list.push_back(title_text);
        auto close_text = SolidLabelElement::Create();
        close_text->SetColor(CColor(Gdiplus::Color::White));
        close_text->SetRenderRect(CRect(r.Width() - 20, 2, r.Width(), 30));
        close_text->SetText(_T("×"));
        close_text->SetFont(f);
        bag.close_text = close_text;
        list.push_back(close_text);
        auto border = RoundBorderElement::Create();
        border->SetColor(CColor(149, 187, 234));
        border->SetFill(false);
        border->SetRadius(0.0f);
        border->SetRenderRect(r);
        bag.border = border;
        list.push_back(border);
        auto client = SolidBackgroundElement::Create();
        client->SetColor(style->get_color(cwindow_style::c_window_background));
        auto border_x = style->get_int(cwindow_style::p_border_x);
        client->SetRenderRect(CRect(border_x, title_y, r.Width() - border_x, r.Height() - border_x));
        bag.client = client;
        list.push_back(client);
        base_id = create_comctl(layout_linear);
        bag.comctl = handles[base_id].comctl;
    }

    bool cwindow::is_border(const CPoint& pt, int& cx, int& cy)
    {
        auto suc = false;
        cx = cy = 0;
        const int border_len = WINDOW_BORDER_X;
        if (abs(pt.x) <= border_len) {
            suc = true;
            cx = -1;
        }
        if (abs(pt.y) <= border_len) {
            suc = true;
            cy = -1;
        }
        if (abs(pt.x - location.Width()) <= border_len) {
            suc = true;
            cx = 1;
        }
        if (abs(pt.y - location.Height()) <= border_len) {
            suc = true;
            cy = 1;
        }
        return suc;
    }

    comctl_base* cwindow::new_comctl(window_comctl_type t)
    {
        switch (t) {
        case layout_absolute: return new cwindow_layout_absolute();
        case layout_linear: return new cwindow_layout_linear();
        case layout_grid: break;
        case comctl_label: return new cwindow_comctl_label();
        case comctl_button: return new cwindow_comctl_button();
        }
        return nullptr;
    }

    void cwindow::error(const string_t& str) const
    {
        vm->error(str);
    }

    void cwindow::destroy()
    {
        while (!handles_set.empty()) {
            destroy_handle(*handles_set.begin(), true);
        }
    }

    void cwindow::destroy_handle(int handle, bool force)
    {
        if (handle < 0 || handle >= WINDOW_HANDLE_NUM)
            error("invalid handle");
        if (!force && handle == base_id)
            error("invalid handle");
        if (handles[handle].type != comctl_none) {
            handles_set.erase(handle);
            auto h = &handles[handle];
            auto c = h->comctl;
            if (c->get_parent() && c->get_parent()->get_layout()) {
                c->get_parent()->get_layout()->remove(c);
            }
            if (c->get_layout()) {
                auto cl = c->get_layout()->get_list();
                for (auto cc : cl) {
                    destroy_handle(cc, force);
                }
            }
            delete h->comctl;
            h->type = comctl_none;
            available_handles--;
            {
                std::stringstream ss;
                ss << "/handle/" << this->handle << "/comctl/" << handle;
                vm->fs.rm(ss.str());
            }
        }
        else {
            error("destroy handle failed!");
        }
    }

    bool cwindow::valid_handle(int h) const
    {
        return handles_set.find(h) != handles_set.end();
    }

    bool cwindow::is_nonclient(const CPoint& pt) const
    {
        if (self_title.PtInRect(pt))
            return true;
        return false;
    }

    void cwindow::set_caption(const string_t& text)
    {
        if (caption == text) return;
        caption = text;
        bag.title_text->SetText(CString(CStringA(caption.c_str())));
    }

    void cwindow::post_data(const int& code, int param1, int param2, int comctl)
    {
        if (comctl == -1) {
            if (code == WM_MOUSEENTER)
            {
                bag.border->SetColor(CColor(36, 125, 234));
                self_hovered = true;
            }
            else if (code == WM_MOUSELEAVE)
            {
                bag.border->SetColor(CColor(149, 187, 234));
                self_hovered = false;
                self_drag = false;
                self_size = false;
            }
            else if (code == WM_SETFOCUS)
            {
                bag.title->SetColor(CColor(45, 120, 213));
                self_focused = true;
            }
            else if (code == WM_KILLFOCUS)
            {
                bag.title->SetColor(CColor(149, 187, 234));
                self_focused = false;
                self_drag = false;
                self_size = false;
            }
            else if (code == WM_NCLBUTTONDOWN)
            {
                int cx, cy;
                if (is_border(CPoint(param1, param2), cx, cy)) {
                    self_size = true;
                }
                else {
                    self_drag = true;
                }
            }
            else if (code == WM_LBUTTONDOWN)
            {
                int cx, cy;
                if (is_border(CPoint(param1, param2 + self_title.Height()), cx, cy)) {
                    self_size = true;
                }
            }
            else if (code == WM_MOUSEMOVE)
            {
                int cx, cy;
                if (is_border(CPoint(param1, param2 + self_title.Height()), cx, cy)) {
                    cursor = sys_cursor(cx, cy);
                }
                else {
                    cursor = 1;
                }
            }
            else if (code == WM_NCMOUSEMOVE)
            {
                int cx, cy;
                if (is_border(CPoint(param1, param2), cx, cy)) {
                    cursor = sys_cursor(cx, cy);
                }
                else {
                    cursor = 1;
                }
            }
            else if (code == WM_NCLBUTTONUP || code == WM_LBUTTONUP)
            {
                self_drag = false;
                if (self_size) {
                    self_size = false;
                    cursor = 1;
                }
            }
        }
        switch (code) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            if (comctl == -1 && param1 != 0 && param2 != 0) {
                comctl = bag.comctl->hit(self_client.x, self_client.y);
            }
            if (code == WM_MOUSEMOVE) {
                if (comctl != -1) {
                    if (comctl_hover != -1) {
                        if (comctl_hover != comctl) {
                            post_data(WM_MOUSELEAVE, 0, 0, comctl_hover);
                            comctl_hover = comctl;
                            post_data(WM_MOUSEENTER, 0, 0, comctl_hover);
                        }
                    }
                    else {
                        comctl_hover = comctl;
                        post_data(WM_MOUSEENTER, 0, 0, comctl_hover);
                    }
                }
                else if (comctl_hover != -1) {
                    post_data(WM_MOUSELEAVE, 0, 0, comctl_hover);
                    comctl_hover = -1;
                }
            }
            else {
                if (comctl != -1) {
                    if (comctl_focus != -1) {
                        if (comctl_focus != comctl) {
                            post_data(WM_KILLFOCUS, 0, 0, comctl_focus);
                            comctl_focus = comctl;
                            post_data(WM_SETFOCUS, 0, 0, comctl_focus);
                        }
                    }
                    else {
                        comctl_focus = comctl;
                        post_data(WM_SETFOCUS, 0, 0, comctl_focus);
                    }
                }
                else if (comctl_focus != -1) {
                    post_data(WM_KILLFOCUS, 0, 0, comctl_focus);
                    comctl_focus = -1;
                }
            }
            break;
        case WM_MOUSEHOVER:
            if (comctl_hover != -1)
                comctl = comctl_hover;
            break;
        }
        window_msg s{ code, comctl, (uint32)param1, (uint32)param2 };
        const auto p = (byte*)& s;
        for (auto i = 0; i < sizeof(window_msg); i++) {
            msg_data.push(p[i]);
        }
    }

    int cwindow::create_comctl(window_comctl_type type)
    {
        if (available_handles >= WINDOW_HANDLE_NUM)
            error("max window handle num!");
        auto end = WINDOW_HANDLE_NUM + handle_ids;
        for (int i = handle_ids; i < end; ++i) {
            auto j = i % WINDOW_HANDLE_NUM;
            if (handles[j].type == comctl_none) {
                handles[j].type = type;
                handles[j].comctl = new_comctl(type);
                ATLASSERT(handles[j].comctl);
                handles[j].comctl->set_id(j);
                handles[j].comctl->set_rt(renderTarget, style);
                handle_ids = (j + 1) % WINDOW_HANDLE_NUM;
                available_handles++;
                handles_set.insert(j);
                {
                    std::stringstream ss;
                    ss << "/handle/" << handle << "/comctl/" << j;
                    auto dir = ss.str();
                    auto& fs = vm->fs;
                    fs.as_root(true);
                    if (fs.mkdir(dir) == 0) { // '/handle/[widnwo id]/comctl/[comctl id]'
                        static std::vector<string_t> ps =
                        { "type", "name" };
                        dir += "/";
                        for (auto& _ps : ps) {
                            ss.str("");
                            ss << dir << _ps;
                            fs.func(ss.str(), vm);
                        }
                    }
                    fs.as_root(false);
                }
                return j;
            }
        }
        error("max window handle num!");
        return -1;
    }

    string_t cwindow::handle_typename(window_comctl_type t)
    {
        static std::tuple<window_comctl_type, string_t> handle_typename_list[] = {
            std::make_tuple(comctl_none, "none"),
            std::make_tuple(layout_absolute, "absolute layout"),
            std::make_tuple(layout_linear, "linear layout"),
            std::make_tuple(layout_grid, "grid layout"),
            std::make_tuple(comctl_label, "label"),
            std::make_tuple(comctl_end, "end"),
        };
        assert(t >= comctl_none && t < comctl_end);
        return std::get<1>(handle_typename_list[t]);
    }

    string_t cwindow::handle_fs(const string_t& path)
    {
        static string_t pat{ R"(/(\d+)/([a-z_]+))" };
        static std::regex re(pat);
        std::smatch res;
        if (std::regex_match(path, res, re)) {
            auto id = std::stoi(res[1].str());
            if (handles[id].type == comctl_none) {
                return "\033FFFF00000\033[ERROR] Invalid handle.\033S4\033";
            }
            const auto& op = res[2].str();
            if (op == "type") {
                return handle_typename(handles[id].type);
            }
            else if (op == "name") {
                return "none";
            }
        }
        return "\033FFFF00000\033[ERROR] Invalid handle.\033S4\033";
    }

    int cwindow::get_base() const
    {
        return base_id;
    }

    bool cwindow::connect(int p, int c)
    {
        if (p != c && valid_handle(p) && valid_handle(c) && !handles[c].comctl->get_parent() && handles[p].comctl->get_layout()) {
            handles[p].comctl->get_layout()->add(handles[c].comctl);
            return true;
        }
        return false;
    }

    bool cwindow::set_bound(int h, const CRect& bound)
    {
        if (valid_handle(h)) {
            handles[h].comctl->set_bound(bound);
            return true;
        }
        return false;
    }

    bool cwindow::set_text(int h, const string_t& text)
    {
        if (valid_handle(h) && handles[h].comctl->get_label()) {
            handles[h].comctl->get_label()->set_text(text);
            return true;
        }
        return false;
    }

    bool cwindow::set_flag(int h, int flag)
    {
        if (valid_handle(h)) {
            handles[h].comctl->set_flag(flag);
            return true;
        }
        return false;
    }

    comctl_base::comctl_base(int type) : type(type)
    {
    }

    void comctl_base::set_rt(std::shared_ptr<Direct2DRenderTarget> rt, cwindow_style::ref)
    {
        this->rt = rt;
    }

    void comctl_base::paint(const CRect& bounds)
    {
    }

    comctl_base* comctl_base::get_parent() const
    {
        return parent;
    }

    cwindow_layout* comctl_base::get_layout()
    {
        return nullptr;
    }

    cwindow_comctl_label* comctl_base::get_label()
    {
        return nullptr;
    }

    void comctl_base::set_bound(const CRect& bound)
    {
        this->bound = bound;
    }

    CRect comctl_base::get_bound() const
    {
        return bound;
    }

    int comctl_base::set_flag(int flag)
    {
        return 0;
    }

    int comctl_base::hit(int x, int y) const
    {
        return -1;
    }

    void comctl_base::set_id(int id)
    {
        this->id = id;
    }

    int comctl_base::get_id() const
    {
        return id;
    }

    int comctl_base::handle_msg(int code, uint32 param1, uint32 param2)
    {
        return 0;
    }

    CSize comctl_base::min_size() const
    {
        return CSize();
    }

    cwindow_layout::cwindow_layout(int type) : comctl_base(type)
    {
    }

    cwindow_layout* cwindow_layout::get_layout()
    {
        return this;
    }

    void cwindow_layout::add(comctl_base* child)
    {
        if (!child->get_parent())
            children.push_back(child);
        else
            ATLVERIFY(!"comctl already has parent!");
    }

    void cwindow_layout::remove(comctl_base* child)
    {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }

    std::vector<int> cwindow_layout::get_list() const
    {
        std::vector<int> v(children.size());
        std::transform(children.begin(), children.end(), v.begin(),
            [](comctl_base* c) -> int { return c->get_id(); });
        return v;
    }

    int cwindow_layout::hit(int x, int y) const
    {
        if (children.empty())
            return -1;
        for (auto c = children.rbegin(); c != children.rend(); c++) {
            auto h = (*c)->hit(x, y);
            if (h) 
                return h;
        }
        return -1;
    }

    cwindow_layout_absolute::cwindow_layout_absolute() : cwindow_layout(cwindow::layout_absolute)
    {
    }

    void cwindow_layout_absolute::paint(const CRect& bounds)
    {
        if (children.empty())
            return;
        for (auto& c : children)
            c->paint(bounds);
    }

    cwindow_layout_linear::cwindow_layout_linear() : cwindow_layout(cwindow::layout_linear)
    {
    }

    void cwindow_layout_linear::paint(const CRect& bounds)
    {
        if (children.empty())
            return;
        auto b = bounds;
        if (align == vertical) {
            for (auto& c : children) {
                c->paint(b);
                b.top = __min(bounds.bottom, b.top + c->get_bound().Height());
                b.bottom = __min(bounds.bottom, b.bottom + c->get_bound().Height());
            }
        } else if (align == horizontal) {
            for (auto& c : children) {
                c->paint(b);
                b.left = __min(bounds.right, b.left + c->get_bound().Width());
                b.right = __min(bounds.right, b.right + c->get_bound().Width());
            }
        }
    }

    int cwindow_layout_linear::set_flag(int flag)
    {
        switch (flag)
        {
        case 0:
            align = vertical;
            break;
        case 1:
            align = horizontal;
            break;
        default:
            break;
        }
        return 0;
    }

    CSize cwindow_layout_linear::min_size() const
    {
        if (children.empty())
            return cwindow_layout::min_size();
        auto b = cwindow_layout::min_size();
        auto lt = children[0]->get_bound().TopLeft();
        if (align == vertical) {
            for (auto& c : children) {
                b.cx = __max(b.cx, c->get_bound().Width());
                b.cy += c->get_bound().Height();
            }
        }
        else if (align == horizontal) {
            for (auto& c : children) {
                b.cx += c->get_bound().Width();
                b.cy = __max(b.cy, c->get_bound().Height());
            }
        }
        return lt + b;
    }

    cwindow_comctl_label::cwindow_comctl_label() : comctl_base(cwindow::comctl_label)
    {
        text = SolidLabelElement::Create();
        text->SetColor(CColor(Gdiplus::Color::Black));
    }

    void cwindow_comctl_label::set_rt(std::shared_ptr<Direct2DRenderTarget> rt, cwindow_style::ref style)
    {
        comctl_base::set_rt(rt, style);
        text->GetRenderer()->SetRenderTarget(rt);
    }

    void cwindow_comctl_label::paint(const CRect& bounds)
    {
        text->SetRenderRect((bound).OfRect(bounds));
        if (bound.Height() > bounds.Height() || bound.Width() > bounds.Width())
            return;
        if (bound.Height() > text->GetRenderRect().Height() || bound.Width() > text->GetRenderRect().Width())
            return;
        text->GetRenderer()->Render(text->GetRenderRect());
    }

    cwindow_comctl_label* cwindow_comctl_label::get_label()
    {
        return this;
    }

    void cwindow_comctl_label::set_text(const string_t& text)
    {
        this->text->SetText(CString(CStringA(text.c_str())));
    }

    int cwindow_comctl_label::set_flag(int flag)
    {
        switch (flag)
        {
        case 10:
            this->text->SetVerticalAlignment(Alignment::StringAlignmentNear);
            break;
        case 11:
            this->text->SetVerticalAlignment(Alignment::StringAlignmentCenter);
            break;
        case 12:
            this->text->SetVerticalAlignment(Alignment::StringAlignmentFar);
            break;
        case 13:
            this->text->SetHorizontalAlignment(Alignment::StringAlignmentNear);
            break;
        case 14:
            this->text->SetHorizontalAlignment(Alignment::StringAlignmentCenter);
            break;
        case 15:
            this->text->SetHorizontalAlignment(Alignment::StringAlignmentFar);
            break;
        default:
            break;
        }
        return 0;
    }

    int cwindow_comctl_label::hit(int x, int y) const
    {
        if (this->text->GetRenderRect().PtInRect(CPoint(x, y)))
            return id;
        return 0;
    }

    int cwindow_comctl_label::handle_msg(int code, uint32 param1, uint32 param2)
    {
        switch (code) {
        case WM_MOUSEENTER:
        {
            auto f = text->GetFont();
            f.underline = true;
            text->SetFont(f);
        }
        break;
        case WM_MOUSELEAVE:
        {
            auto f = text->GetFont();
            f.underline = false;
            text->SetFont(f);
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        {
            auto f = text->GetFont();
            f.bold = true;
            text->SetFont(f);
        }
        break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        {
            auto f = text->GetFont();
            f.bold = false;
            text->SetFont(f);
        }
        break;
        }
        return 0;
    }

    CSize cwindow_comctl_label::min_size() const
    {
        return text->GetRenderer()->GetMinSize();
    }

    cwindow_comctl_button::cwindow_comctl_button()
    {
        type = cwindow::comctl_button;
        background = RoundBorderElement::Create();
    }

    void cwindow_comctl_button::set_rt(std::shared_ptr<Direct2DRenderTarget> rt, cwindow_style::ref style)
    {
        cwindow_comctl_label::set_rt(rt, style);
        _style = style;
        background->GetRenderer()->SetRenderTarget(rt);
        background->SetColor(style->get_color(cwindow_style::c_button_bg_def));
        background->SetRadius(style->get_float(cwindow_style::f_button_radius));
        text->SetColor(_style.lock()->get_color(cwindow_style::c_button_fg_def));
    }

    void cwindow_comctl_button::paint(const CRect& bounds)
    {
        background->SetRenderRect((bound).OfRect(bounds));
        if (bound.Height() > bounds.Height() || bound.Width() > bounds.Width())
            return;
        if (bound.Height() > background->GetRenderRect().Height() || bound.Width() > background->GetRenderRect().Width())
            return;
        background->GetRenderer()->Render(background->GetRenderRect());
        cwindow_comctl_label::paint(bounds);
    }

    int cwindow_comctl_button::hit(int x, int y) const
    {
        if (this->background->GetRenderRect().PtInRect(CPoint(x, y)))
            return id;
        return 0;
    }

    int cwindow_comctl_button::handle_msg(int code, uint32 param1, uint32 param2)
    {
        switch (code) {
        case WM_MOUSEENTER:
        {
            auto f = text->GetFont();
            f.underline = true;
            text->SetFont(f);
            text->SetColor(_style.lock()->get_color(cwindow_style::c_button_fg_hover));
        }
        break;
        case WM_MOUSELEAVE:
        {
            auto f = text->GetFont();
            f.underline = false;
            text->SetFont(f);
            text->SetColor(_style.lock()->get_color(cwindow_style::c_button_fg_def));
        }
        break;
        case WM_LBUTTONDOWN:
        {
            auto f = text->GetFont();
            f.bold = true;
            text->SetFont(f);
            background->SetColor(_style.lock()->get_color(cwindow_style::c_button_bg_focus));
        }
        break;
        case WM_LBUTTONUP:
        {
            auto f = text->GetFont();
            f.bold = false;
            text->SetFont(f);
            background->SetColor(_style.lock()->get_color(cwindow_style::c_button_bg_def));
        }
        break;
        }
        return 0;
    }

    CSize cwindow_comctl_button::min_size() const
    {
        return background->GetRenderRect().Size();
    }
}