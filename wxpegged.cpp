// wxpegged.cpp -- Portable wxWidgets port of Pegged
//
// Original Win32 game: (c) Mike Blaylock, 1989-1990
// wxWidgets port preserves original game logic but uses portable
// buffered drawing instead of the original's manual GDI bitmap caching.
//
// Build (Unix/macOS):  see GNUmakefile.wx  or  CMakeLists.txt

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/dcbuffer.h>
#include <wx/aboutdlg.h>
#include <wx/config.h>
#include <wx/iconbndl.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

// ---------------------------------------------------------------------------
// IDs and pattern enum
// ---------------------------------------------------------------------------

static const int DIVISIONS = 7;

enum PatternId {
    PAT_CROSS = 0,
    PAT_PLUS,
    PAT_FIREPLACE,
    PAT_UPARROW,
    PAT_PYRAMID,
    PAT_DIAMOND,
    PAT_SOLITAIRE,
    PAT_COUNT
};

enum {
    ID_NEW_GAME = wxID_HIGHEST + 1,
    ID_UNDO_MOVE,
    ID_PATTERN_FIRST,
    ID_PATTERN_CROSS = ID_PATTERN_FIRST,
    ID_PATTERN_PLUS,
    ID_PATTERN_FIREPLACE,
    ID_PATTERN_UPARROW,
    ID_PATTERN_PYRAMID,
    ID_PATTERN_DIAMOND,
    ID_PATTERN_SOLITAIRE,
    ID_PATTERN_LAST = ID_PATTERN_SOLITAIRE,
    ID_HELP_HOWTO
};

static const wxString kPatternNames[PAT_COUNT] = {
    wxT("Cross"), wxT("Plus"), wxT("Fireplace"), wxT("Up Arrow"),
    wxT("Pyramid"), wxT("Diamond"), wxT("Solitaire")
};

struct Move {
    int xsource, ysource;
    int xdest, ydest;
    int xjumped, yjumped;
};

// ---------------------------------------------------------------------------
// Board panel
// ---------------------------------------------------------------------------

class PeggedFrame;

class PeggedBoard : public wxPanel
{
public:
    explicit PeggedBoard(PeggedFrame* parent);

    void NewGame();
    void SetPattern(PatternId p);
    PatternId GetPattern() const { return m_pattern; }
    void Undo();
    bool CanUndo() const { return m_moveCount > 0; }
    int  PegsRemaining() const;

private:
    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnEraseBackground(wxEraseEvent&) { /* suppress flicker */ }
    void OnLeftDown(wxMouseEvent&);
    void OnLeftUp(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnCaptureLost(wxMouseCaptureLostEvent&);

    void ComputeMetrics();
    static bool IsPlayable(int x, int y) {
        return x >= 0 && x < DIVISIONS && y >= 0 && y < DIVISIONS
            && ((x > 1 && x < 5) || (y > 1 && y < 5));
    }
    bool CellFromPixel(int px, int py, int& cx, int& cy) const;

    void DrawBoard(wxDC& dc);
    void DrawPegAtCell(wxDC& dc, int gx, int gy);
    void DrawFloatingPeg(wxDC& dc, int cx, int cy);

    bool CheckValidMove(int sx, int sy, int dx, int dy,
                        int& jx, int& jy) const;
    bool AnyMovesLeft() const;

    void ApplyPattern(PatternId p);
    void NotifyStatusChanged();

    PeggedFrame* m_frame;

    bool m_state[DIVISIONS][DIVISIONS];
    PatternId m_pattern;

    struct Move m_moves[64];
    int m_moveCount;

    // layout metrics, recomputed on resize
    int m_xBlock, m_yBlock;
    int m_xClient, m_yClient;
    int m_xEdge, m_yEdge;
    int m_xShadow, m_yShadow;
    int m_winxEdge, m_winyEdge;

    // drag state
    bool m_dragging;
    int  m_dragSrcX, m_dragSrcY;
    wxPoint m_dragMouse;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(PeggedBoard, wxPanel)
    EVT_PAINT(PeggedBoard::OnPaint)
    EVT_SIZE(PeggedBoard::OnSize)
    EVT_ERASE_BACKGROUND(PeggedBoard::OnEraseBackground)
    EVT_LEFT_DOWN(PeggedBoard::OnLeftDown)
    EVT_LEFT_UP(PeggedBoard::OnLeftUp)
    EVT_MOTION(PeggedBoard::OnMotion)
    EVT_MOUSE_CAPTURE_LOST(PeggedBoard::OnCaptureLost)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// Main frame
// ---------------------------------------------------------------------------

class PeggedFrame : public wxFrame
{
public:
    PeggedFrame();

    void UpdateStatus();

private:
    void OnNewGame(wxCommandEvent&);
    void OnUndo(wxCommandEvent&);
    void OnExit(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnHowTo(wxCommandEvent&);
    void OnPattern(wxCommandEvent&);
    void OnUpdateUndo(wxUpdateUIEvent&);
    void OnClose(wxCloseEvent&);

    PeggedBoard* m_board;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(PeggedFrame, wxFrame)
    EVT_MENU(ID_NEW_GAME,        PeggedFrame::OnNewGame)
    EVT_MENU(ID_UNDO_MOVE,       PeggedFrame::OnUndo)
    EVT_MENU(wxID_EXIT,          PeggedFrame::OnExit)
    EVT_MENU(wxID_ABOUT,         PeggedFrame::OnAbout)
    EVT_MENU(ID_HELP_HOWTO,      PeggedFrame::OnHowTo)
    EVT_MENU_RANGE(ID_PATTERN_FIRST, ID_PATTERN_LAST, PeggedFrame::OnPattern)
    EVT_UPDATE_UI(ID_UNDO_MOVE,  PeggedFrame::OnUpdateUndo)
    EVT_CLOSE(                   PeggedFrame::OnClose)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------

class PeggedApp : public wxApp
{
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(PeggedApp);

bool PeggedApp::OnInit()
{
    if (!wxApp::OnInit()) return false;
    SetAppName(wxT("Pegged"));
    SetVendorName(wxT("Pegged"));
    wxInitAllImageHandlers();
    auto* frame = new PeggedFrame();
    frame->Show(true);
    return true;
}

// ---------------------------------------------------------------------------
// PeggedFrame implementation
// ---------------------------------------------------------------------------

// Build an icon bundle from whichever assets we can find at runtime:
// the installed hicolor tree, the source tree (for uninstalled dev runs),
// or the macOS bundle's Resources directory. Multiple sizes let the
// window manager pick the sharpest for each use (title bar, Alt-Tab, etc.).
static wxIconBundle LoadAppIcons()
{
    wxIconBundle icons;

    wxArrayString dirs;

    // Next to / below the executable (dev builds, portable installs).
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    const wxString exeDir = exe.GetPath();
    dirs.Add(exeDir);
    dirs.Add(exeDir + wxT("/sizes"));
    dirs.Add(exeDir + wxT("/../sizes"));
    dirs.Add(exeDir + wxT("/../share/icons/hicolor"));

    // macOS .app bundle Resources.
    dirs.Add(wxStandardPaths::Get().GetResourcesDir());

    // Installed hicolor theme locations.
    dirs.Add(wxT("/usr/share/icons/hicolor"));
    dirs.Add(wxT("/usr/local/share/icons/hicolor"));

    static const wxChar* const sizeDirs[] = {
        wxT("16x16/apps"),  wxT("22x22/apps"),  wxT("24x24/apps"),
        wxT("32x32/apps"),  wxT("48x48/apps"),  wxT("64x64/apps"),
        wxT("96x96/apps"),  wxT("128x128/apps"), wxT("256x256/apps")
    };
    static const wxChar* const basenames[] = {
        wxT("wxpegged.png"), wxT("pegged.png")
    };

    for (size_t d = 0; d < dirs.size(); ++d) {
        for (size_t s = 0; s < WXSIZEOF(sizeDirs); ++s) {
            for (size_t b = 0; b < WXSIZEOF(basenames); ++b) {
                const wxString path = dirs[d] + wxT("/") + sizeDirs[s]
                                    + wxT("/") + basenames[b];
                if (wxFileName::FileExists(path)) {
                    wxIcon ic;
                    if (ic.LoadFile(path, wxBITMAP_TYPE_PNG))
                        icons.AddIcon(ic);
                }
            }
        }
    }

    // Fall back to the Windows .ico or macOS .icns sitting next to the exe.
    static const wxChar* const flatNames[] = {
        wxT("pegged.ico"), wxT("pegged.icns")
    };
    for (size_t i = 0; i < WXSIZEOF(flatNames); ++i) {
        const wxString path = exeDir + wxT("/") + flatNames[i];
        if (wxFileName::FileExists(path)) {
            wxIcon ic;
            if (ic.LoadFile(path, wxBITMAP_TYPE_ANY))
                icons.AddIcon(ic);
        }
    }

    return icons;
}

PeggedFrame::PeggedFrame()
    : wxFrame(nullptr, wxID_ANY, wxT("Pegged"),
              wxDefaultPosition, wxSize(480, 520))
{
    // --- Window icon ------------------------------------------------------
#ifdef __WXMSW__
    // On Windows, prefer the icon compiled into the .rc resources.
    SetIcon(wxIcon(wxT("100"))); // numeric id from pegged.rc
#endif
    wxIconBundle icons = LoadAppIcons();
    if (!icons.IsEmpty())
        SetIcons(icons);

    // --- Menu bar ---------------------------------------------------------
    auto* menuBar = new wxMenuBar();

    auto* gameMenu = new wxMenu();
    gameMenu->Append(ID_NEW_GAME,  wxT("&New\tF2"));
    gameMenu->Append(ID_UNDO_MOVE, wxT("&Backup\tCtrl+Z"));
    gameMenu->AppendSeparator();
    gameMenu->Append(wxID_EXIT,    wxT("E&xit\tAlt+F4"));
    menuBar->Append(gameMenu, wxT("&Game"));

    auto* optMenu = new wxMenu();
    optMenu->AppendRadioItem(ID_PATTERN_CROSS,     wxT("&Cross"));
    optMenu->AppendRadioItem(ID_PATTERN_PLUS,      wxT("&Plus"));
    optMenu->AppendRadioItem(ID_PATTERN_FIREPLACE, wxT("&Fireplace"));
    optMenu->AppendRadioItem(ID_PATTERN_UPARROW,   wxT("Up &Arrow"));
    optMenu->AppendRadioItem(ID_PATTERN_PYRAMID,   wxT("P&yramid"));
    optMenu->AppendRadioItem(ID_PATTERN_DIAMOND,   wxT("&Diamond"));
    optMenu->AppendRadioItem(ID_PATTERN_SOLITAIRE, wxT("&Solitaire"));
    menuBar->Append(optMenu, wxT("&Options"));

    auto* helpMenu = new wxMenu();
    helpMenu->Append(ID_HELP_HOWTO, wxT("&How to Play\tF1"));
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT,    wxT("&About Pegged..."));
    menuBar->Append(helpMenu, wxT("&Help"));

    SetMenuBar(menuBar);

    // --- Accelerator table -----------------------------------------------
    wxAcceleratorEntry entries[3];
    entries[0].Set(wxACCEL_NORMAL, WXK_F2,  ID_NEW_GAME);
    entries[1].Set(wxACCEL_NORMAL, WXK_F1,  ID_HELP_HOWTO);
    entries[2].Set(wxACCEL_NORMAL, WXK_BACK, ID_UNDO_MOVE);
    SetAcceleratorTable(wxAcceleratorTable(3, entries));

    // --- Status bar -------------------------------------------------------
    CreateStatusBar(2);
    SetStatusText(wxT("Ready"), 0);

    // --- Board ------------------------------------------------------------
    m_board = new PeggedBoard(this);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_board, 1, wxEXPAND);
    SetSizer(sizer);

    // --- Restore preferred pattern from config ---------------------------
    wxConfigBase* cfg = wxConfigBase::Get();
    long patInt = 0;
    cfg->Read(wxT("/Pegged/Pattern"), &patInt, (long)PAT_CROSS);
    if (patInt < 0 || patInt >= PAT_COUNT) patInt = PAT_CROSS;
    menuBar->Check(ID_PATTERN_FIRST + (int)patInt, true);
    m_board->SetPattern((PatternId)patInt);

    SetMinClientSize(wxSize(300, 300));
    Centre();
    UpdateStatus();
}

void PeggedFrame::UpdateStatus()
{
    int p = (int)m_board->GetPattern();
    SetStatusText(wxString::Format(wxT("Pattern: %s"),
                                   kPatternNames[p]), 0);
    SetStatusText(wxString::Format(wxT("Pegs remaining: %d"),
                                   m_board->PegsRemaining()), 1);
}

void PeggedFrame::OnNewGame(wxCommandEvent&) { m_board->NewGame(); UpdateStatus(); }
void PeggedFrame::OnUndo(wxCommandEvent&)    { m_board->Undo();    UpdateStatus(); }
void PeggedFrame::OnExit(wxCommandEvent&)    { Close(true); }

void PeggedFrame::OnPattern(wxCommandEvent& evt)
{
    int id = evt.GetId() - ID_PATTERN_FIRST;
    if (id >= 0 && id < PAT_COUNT) {
        m_board->SetPattern((PatternId)id);
        UpdateStatus();
    }
}

void PeggedFrame::OnUpdateUndo(wxUpdateUIEvent& evt)
{
    evt.Enable(m_board->CanUndo());
}

void PeggedFrame::OnHowTo(wxCommandEvent&)
{
    wxMessageBox(
        wxT("Pegged is a peg-jumping solitaire game.\n\n")
        wxT("Drag a peg over an adjacent peg and drop it into the empty\n")
        wxT("hole on the far side. The jumped peg is removed.\n\n")
        wxT("Jumps must be horizontal or vertical (no diagonals).\n\n")
        wxT("Choose a starting pattern from the Options menu. The goal\n")
        wxT("(on Solitaire) is to leave a single peg in the center."),
        wxT("How to Play"), wxOK | wxICON_INFORMATION, this);
}

void PeggedFrame::OnAbout(wxCommandEvent&)
{
    wxAboutDialogInfo info;
    info.SetName(wxT("Pegged"));
    info.SetVersion(wxT("1.0 (wxWidgets port)"));
    info.SetDescription(wxT("A portable peg-solitaire game."));
    info.SetCopyright(wxT("(C) 1989-1990 Mike Blaylock\n")
                      wxT("wxWidgets port 2026"));
    wxAboutBox(info, this);
}

void PeggedFrame::OnClose(wxCloseEvent& evt)
{
    wxConfigBase* cfg = wxConfigBase::Get();
    cfg->Write(wxT("/Pegged/Pattern"), (long)m_board->GetPattern());
    cfg->Flush();
    evt.Skip();
}

// ---------------------------------------------------------------------------
// PeggedBoard implementation
// ---------------------------------------------------------------------------

PeggedBoard::PeggedBoard(PeggedFrame* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(400, 400),
              wxFULL_REPAINT_ON_RESIZE),
      m_frame(parent),
      m_pattern(PAT_CROSS),
      m_moveCount(0),
      m_xBlock(1), m_yBlock(1),
      m_xClient(0), m_yClient(0),
      m_xEdge(0), m_yEdge(0),
      m_xShadow(0), m_yShadow(0),
      m_winxEdge(0), m_winyEdge(0),
      m_dragging(false),
      m_dragSrcX(0), m_dragSrcY(0)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(192, 192, 192));
    for (int i = 0; i < DIVISIONS; ++i)
        for (int j = 0; j < DIVISIONS; ++j)
            m_state[i][j] = false;
    ApplyPattern(m_pattern);
}

void PeggedBoard::ApplyPattern(PatternId p)
{
    for (int i = 0; i < DIVISIONS; ++i)
        for (int j = 0; j < DIVISIONS; ++j)
            m_state[i][j] = false;

    auto set = [&](int x, int y) {
        if (IsPlayable(x, y)) m_state[x][y] = true;
    };

    switch (p) {
    case PAT_CROSS:
        set(2,2);
        set(3,1); set(3,2); set(3,3); set(3,4);
        set(4,2);
        break;
    case PAT_PLUS:
        set(1,3); set(2,3);
        set(3,1); set(3,2); set(3,3); set(3,4); set(3,5);
        set(4,3); set(5,3);
        break;
    case PAT_FIREPLACE:
        set(2,0); set(2,1); set(2,2); set(2,3);
        set(3,0); set(3,1); set(3,2);
        set(4,0); set(4,1); set(4,2); set(4,3);
        break;
    case PAT_UPARROW:
        set(1,2);
        set(2,1); set(2,2); set(2,5); set(2,6);
        set(3,0); set(3,1); set(3,2); set(3,3); set(3,4); set(3,5); set(3,6);
        set(4,1); set(4,2); set(4,5); set(4,6);
        set(5,2);
        break;
    case PAT_PYRAMID:
        set(0,4);
        set(1,3); set(1,4);
        set(2,2); set(2,3); set(2,4);
        set(3,1); set(3,2); set(3,3); set(3,4);
        set(4,2); set(4,3); set(4,4);
        set(5,3); set(5,4);
        set(6,4);
        break;
    case PAT_DIAMOND:
        set(0,3);
        set(1,2); set(1,3); set(1,4);
        set(2,1); set(2,2); set(2,3); set(2,4); set(2,5);
        set(3,0); set(3,1); set(3,2); set(3,4); set(3,5); set(3,6);
        set(4,1); set(4,2); set(4,3); set(4,4); set(4,5);
        set(5,2); set(5,3); set(5,4);
        set(6,3);
        break;
    case PAT_SOLITAIRE:
        for (int x = 0; x < DIVISIONS; ++x)
            for (int y = 0; y < DIVISIONS; ++y)
                if (IsPlayable(x, y) && !(x == 3 && y == 3))
                    m_state[x][y] = true;
        break;
    default:
        break;
    }
}

void PeggedBoard::NewGame()
{
    ApplyPattern(m_pattern);
    m_moveCount = 0;
    m_dragging = false;
    if (HasCapture()) ReleaseMouse();
    Refresh();
}

void PeggedBoard::SetPattern(PatternId p)
{
    m_pattern = p;
    NewGame();
}

int PeggedBoard::PegsRemaining() const
{
    int n = 0;
    for (int x = 0; x < DIVISIONS; ++x)
        for (int y = 0; y < DIVISIONS; ++y)
            if (IsPlayable(x, y) && m_state[x][y]) ++n;
    return n;
}

void PeggedBoard::Undo()
{
    if (m_moveCount <= 0) return;
    --m_moveCount;
    const struct Move& mv = m_moves[m_moveCount];
    m_state[mv.xsource][mv.ysource] = true;
    m_state[mv.xjumped][mv.yjumped] = true;
    m_state[mv.xdest][mv.ydest]     = false;
    Refresh();
}

void PeggedBoard::NotifyStatusChanged()
{
    if (m_frame) m_frame->UpdateStatus();
}

// ---------------------------------------------------------------------------
// Layout / hit-testing
// ---------------------------------------------------------------------------

void PeggedBoard::ComputeMetrics()
{
    wxSize sz = GetClientSize();
    // Keep the board square by using the smaller dimension.
    int s = wxMin(sz.GetWidth(), sz.GetHeight());
    if (s < 9) s = 9;
    m_xClient = m_yClient = s;
    m_xBlock = m_yBlock = s / (DIVISIONS + 2);
    if (m_xBlock < 1) m_xBlock = 1;
    if (m_yBlock < 1) m_yBlock = 1;
    m_xEdge    = wxMax(1, m_xBlock / 4);
    m_yEdge    = wxMax(1, m_yBlock / 4);
    m_xShadow  = wxMax(1, m_xBlock / 10);
    m_yShadow  = wxMax(1, m_yBlock / 10);
    m_winxEdge = m_xBlock;
    m_winyEdge = m_yBlock;
}

bool PeggedBoard::CellFromPixel(int px, int py, int& cx, int& cy) const
{
    if (px < m_winxEdge || py < m_winyEdge) return false;
    if (px >= m_winxEdge + DIVISIONS * m_xBlock) return false;
    if (py >= m_winyEdge + DIVISIONS * m_yBlock) return false;
    cx = (px - m_winxEdge) / m_xBlock;
    cy = (py - m_winyEdge) / m_yBlock;
    return IsPlayable(cx, cy);
}

// ---------------------------------------------------------------------------
// Move validation
// ---------------------------------------------------------------------------

bool PeggedBoard::CheckValidMove(int sx, int sy, int dx, int dy,
                                 int& jx, int& jy) const
{
    if (!IsPlayable(sx, sy) || !IsPlayable(dx, dy)) return false;
    int dxv = dx - sx, dyv = dy - sy;
    if (dxv == 0 && (dyv == 2 || dyv == -2)) {
        jx = sx; jy = sy + dyv / 2;
    } else if (dyv == 0 && (dxv == 2 || dxv == -2)) {
        jx = sx + dxv / 2; jy = sy;
    } else {
        return false;
    }
    if (!IsPlayable(jx, jy)) return false;
    if (!m_state[sx][sy]) return false;
    if ( m_state[dx][dy]) return false;
    if (!m_state[jx][jy]) return false;
    return true;
}

bool PeggedBoard::AnyMovesLeft() const
{
    static const int dxs[] = { 0,  0,  2, -2 };
    static const int dys[] = { 2, -2,  0,  0 };
    for (int x = 0; x < DIVISIONS; ++x) {
        for (int y = 0; y < DIVISIONS; ++y) {
            if (!IsPlayable(x, y) || !m_state[x][y]) continue;
            for (int i = 0; i < 4; ++i) {
                int nx = x + dxs[i], ny = y + dys[i];
                int mx = x + dxs[i] / 2, my = y + dys[i] / 2;
                if (IsPlayable(nx, ny) && IsPlayable(mx, my)
                    && !m_state[nx][ny] && m_state[mx][my])
                    return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void PeggedBoard::DrawBoard(wxDC& dc)
{
    // Background
    dc.SetPen(wxPen(wxColour(192, 192, 192)));
    dc.SetBrush(wxBrush(wxColour(192, 192, 192)));
    dc.DrawRectangle(0, 0, GetClientSize().GetWidth(),
                     GetClientSize().GetHeight());

    // Shadow lines around the plus-shaped playing area (right and bottom).
    wxPen thickDark(wxColour(64, 64, 64), 2);
    dc.SetPen(thickDark);
    // bottom edge of left arm
    dc.DrawLine(m_winxEdge,                 m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 2 * m_xBlock,  m_winyEdge + 5 * m_yBlock + 1);
    // bottom edge of middle column
    dc.DrawLine(m_winxEdge + 2 * m_xBlock,  m_winyEdge + 7 * m_yBlock + 1,
                m_winxEdge + 5 * m_xBlock,  m_winyEdge + 7 * m_yBlock + 1);
    // bottom edge of right arm
    dc.DrawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1);
    // right edge of top arm
    dc.DrawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge,
                m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 2 * m_yBlock);
    // right edge of middle row (continuation)
    dc.DrawLine(m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 2 * m_yBlock,
                m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1);
    // right edge of bottom arm
    dc.DrawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 7 * m_yBlock + 1);

    // Highlight lines (white) - top and left edges
    dc.SetPen(wxPen(*wxWHITE, 2));
    dc.DrawLine(m_winxEdge - 1, m_winyEdge + 2 * m_yBlock,
                m_winxEdge - 1, m_winyEdge + 5 * m_yBlock - 1);
    dc.DrawLine(m_winxEdge,     m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 2 * m_yBlock - 1);
    dc.DrawLine(m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge);
    dc.DrawLine(m_winxEdge + 2 * m_xBlock,     m_winyEdge - 1,
                m_winxEdge + 5 * m_xBlock - 1, m_winyEdge - 1);
    dc.DrawLine(m_winxEdge + 5 * m_xBlock + 2, m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 7 * m_xBlock,     m_winyEdge + 2 * m_yBlock - 1);
    dc.DrawLine(m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 7 * m_yBlock - 1);

    // Holes
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    for (int x = 0; x < DIVISIONS; ++x) {
        for (int y = 0; y < DIVISIONS; ++y) {
            if (!IsPlayable(x, y)) continue;
            int px = x * m_xBlock + m_winxEdge + m_xEdge;
            int py = y * m_yBlock + m_winyEdge + m_yEdge;
            int w  = m_xBlock - 2 * m_xEdge;
            int h  = m_yBlock - 2 * m_yEdge;
            dc.DrawEllipse(px, py, w, h);
        }
    }
}

void PeggedBoard::DrawPegAtCell(wxDC& dc, int gx, int gy)
{
    int x = gx * m_xBlock + m_winxEdge;
    int y = gy * m_yBlock + m_winyEdge;
    int w = m_xBlock - 2 * m_xEdge;
    int h = m_yBlock - 2 * m_yEdge;

    // shadow
    dc.SetPen(wxPen(wxColour(64, 64, 64), 1));
    dc.SetBrush(wxBrush(wxColour(64, 64, 64)));
    dc.DrawEllipse(x + m_xEdge + m_xShadow, y + m_yEdge + m_yShadow, w, h);

    // peg body
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(wxBrush(wxColour(0, 0, 255)));
    dc.DrawEllipse(x + m_xEdge, y + m_yEdge, w, h);

    // highlight (offset inner ellipse, white, open arc look)
    dc.SetPen(wxPen(*wxWHITE, 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawEllipse(x + m_xEdge + m_xShadow, y + m_yEdge + m_yShadow,
                   w - 2 * m_xShadow, h - 2 * m_yShadow);
}

void PeggedBoard::DrawFloatingPeg(wxDC& dc, int cx, int cy)
{
    int w = m_xBlock - 2 * m_xEdge;
    int h = m_yBlock - 2 * m_yEdge;

    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(wxBrush(wxColour(0, 0, 255)));
    dc.DrawEllipse(cx - w / 2, cy - h / 2, w, h);

    dc.SetPen(wxPen(*wxWHITE, 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawEllipse(cx - w / 2 + m_xShadow,
                   cy - h / 2 + m_yShadow,
                   w - 2 * m_xShadow, h - 2 * m_yShadow);
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void PeggedBoard::OnSize(wxSizeEvent& evt)
{
    ComputeMetrics();
    Refresh();
    evt.Skip();
}

void PeggedBoard::OnPaint(wxPaintEvent&)
{
    ComputeMetrics();
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(192, 192, 192)));
    dc.Clear();

    DrawBoard(dc);

    // Draw pegs. If dragging, skip the source cell and draw the dragged
    // peg at the current mouse position on top of everything else.
    for (int x = 0; x < DIVISIONS; ++x) {
        for (int y = 0; y < DIVISIONS; ++y) {
            if (!IsPlayable(x, y) || !m_state[x][y]) continue;
            if (m_dragging && x == m_dragSrcX && y == m_dragSrcY) continue;
            DrawPegAtCell(dc, x, y);
        }
    }

    if (m_dragging) {
        DrawFloatingPeg(dc, m_dragMouse.x, m_dragMouse.y);
    }
}

void PeggedBoard::OnLeftDown(wxMouseEvent& evt)
{
    int cx, cy;
    if (!CellFromPixel(evt.GetX(), evt.GetY(), cx, cy)) { evt.Skip(); return; }
    if (!m_state[cx][cy]) { evt.Skip(); return; }

    m_dragging = true;
    m_dragSrcX = cx;
    m_dragSrcY = cy;
    m_dragMouse = evt.GetPosition();
    CaptureMouse();
    SetCursor(wxCURSOR_SIZING);
    Refresh();
}

void PeggedBoard::OnMotion(wxMouseEvent& evt)
{
    if (!m_dragging) return;
    m_dragMouse = evt.GetPosition();
    Refresh();
}

void PeggedBoard::OnLeftUp(wxMouseEvent& evt)
{
    if (!m_dragging) { evt.Skip(); return; }

    m_dragging = false;
    if (HasCapture()) ReleaseMouse();
    SetCursor(wxNullCursor);

    int cx, cy;
    if (CellFromPixel(evt.GetX(), evt.GetY(), cx, cy)) {
        int jx, jy;
        if (CheckValidMove(m_dragSrcX, m_dragSrcY, cx, cy, jx, jy)) {
            // Record & apply move.
            if (m_moveCount < (int)(sizeof(m_moves) / sizeof(m_moves[0]))) {
                struct Move& mv = m_moves[m_moveCount++];
                mv.xsource  = m_dragSrcX; mv.ysource  = m_dragSrcY;
                mv.xdest    = cx;         mv.ydest    = cy;
                mv.xjumped  = jx;         mv.yjumped  = jy;
            }
            m_state[m_dragSrcX][m_dragSrcY] = false;
            m_state[jx][jy]                 = false;
            m_state[cx][cy]                 = true;

            Refresh();
            NotifyStatusChanged();

            if (!AnyMovesLeft()) {
                int remaining = PegsRemaining();
                wxString msg = (remaining <= 1)
                    ? wxT("You Win!")
                    : wxString::Format(wxT("Game Over.\n%d peg%s left."),
                                       remaining, remaining == 1 ? wxT("") : wxT("s"));
                wxMessageBox(msg, wxT("Pegged"),
                             wxOK | wxICON_INFORMATION, this);
            }
            return;
        }
    }

    // invalid drop: snap peg back
    Refresh();
}

void PeggedBoard::OnCaptureLost(wxMouseCaptureLostEvent&)
{
    m_dragging = false;
    SetCursor(wxNullCursor);
    Refresh();
}
