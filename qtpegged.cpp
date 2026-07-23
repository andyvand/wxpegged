// qtpegged.cpp -- Qt Widgets port of Pegged
//
// Original Win32 game: (c) Mike Blaylock, 1989-1990
// Qt port shares the portable game logic with the wxWidgets port
// (wxpegged.cpp) but drives the UI with Qt Widgets so the same game
// can be packaged for Android with androiddeployqt.
//
// Build:  see CMakeLists.txt  (configure with -DUSE_QT=ON; Android forces it on)

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QRect>
#include <QCursor>
#include <cstring>

// ---------------------------------------------------------------------------
// IDs and pattern enum  (shared game model, ported verbatim from wxpegged.cpp)
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

static const char* const kPatternNames[PAT_COUNT] = {
    "Cross", "Plus", "Fireplace", "Up Arrow",
    "Pyramid", "Diamond", "Solitaire"
};

struct Move {
    int xsource, ysource;
    int xdest, ydest;
    int xjumped, yjumped;
};

// ---------------------------------------------------------------------------
// Board widget
// ---------------------------------------------------------------------------

class PeggedBoard : public QWidget
{
    Q_OBJECT
public:
    explicit PeggedBoard(QWidget* parent = nullptr);

    void NewGame();
    void SetPattern(PatternId p);
    PatternId GetPattern() const { return m_pattern; }
    void Undo();
    bool CanUndo() const { return m_moveCount > 0; }
    int  PegsRemaining() const;

signals:
    void stateChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void ComputeMetrics();
    static bool IsPlayable(int x, int y) {
        return x >= 0 && x < DIVISIONS && y >= 0 && y < DIVISIONS
            && ((x > 1 && x < 5) || (y > 1 && y < 5));
    }
    bool CellFromPixel(int px, int py, int& cx, int& cy) const;

    void DrawBoard(QPainter& dc);
    void DrawPegAtCell(QPainter& dc, int gx, int gy);
    void DrawFloatingPeg(QPainter& dc, int cx, int cy);

    bool CheckValidMove(int sx, int sy, int dx, int dy,
                        int& jx, int& jy) const;
    bool AnyMovesLeft() const;

    void ApplyPattern(PatternId p);

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
    QPoint m_dragMouse;
};

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------

class PeggedWindow : public QMainWindow
{
    Q_OBJECT
public:
    PeggedWindow();

    void UpdateStatus();

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void OnNewGame();
    void OnUndo();
    void OnAbout();
    void OnHowTo();
    void OnPattern(int id);

private:
    PeggedBoard* m_board;
    QAction*     m_undoAction;
    QAction*     m_patternActions[PAT_COUNT];
    QLabel*      m_pegLabel;
};

// ---------------------------------------------------------------------------
// PeggedWindow implementation
// ---------------------------------------------------------------------------

PeggedWindow::PeggedWindow()
{
    setWindowTitle(QStringLiteral("Pegged"));

    // --- Board (central widget) ------------------------------------------
    m_board = new PeggedBoard(this);
    setCentralWidget(m_board);
    connect(m_board, &PeggedBoard::stateChanged, this, &PeggedWindow::UpdateStatus);

    // --- Menu bar ---------------------------------------------------------
    QMenu* gameMenu = menuBar()->addMenu(QStringLiteral("&Game"));
    QAction* newAction = gameMenu->addAction(QStringLiteral("&New"),
                                             this, &PeggedWindow::OnNewGame);
    newAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_undoAction = gameMenu->addAction(QStringLiteral("&Backup"),
                                       this, &PeggedWindow::OnUndo);
    m_undoAction->setShortcut(QKeySequence::Undo); // Ctrl+Z
    gameMenu->addSeparator();
    QAction* exitAction = gameMenu->addAction(QStringLiteral("E&xit"),
                                              this, &QWidget::close);
    exitAction->setMenuRole(QAction::QuitRole);

    QMenu* optMenu = menuBar()->addMenu(QStringLiteral("&Options"));
    QActionGroup* patGroup = new QActionGroup(this);
    patGroup->setExclusive(true);
    static const char* const optLabels[PAT_COUNT] = {
        "&Cross", "&Plus", "&Fireplace", "Up &Arrow",
        "P&yramid", "&Diamond", "&Solitaire"
    };
    for (int i = 0; i < PAT_COUNT; ++i) {
        QAction* a = optMenu->addAction(QString::fromUtf8(optLabels[i]));
        a->setCheckable(true);
        patGroup->addAction(a);
        m_patternActions[i] = a;
        connect(a, &QAction::triggered, this, [this, i]() { OnPattern(i); });
    }

    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction* howto = helpMenu->addAction(QStringLiteral("&How to Play"),
                                         this, &PeggedWindow::OnHowTo);
    howto->setShortcut(QKeySequence(Qt::Key_F1));
    helpMenu->addSeparator();
    QAction* about = helpMenu->addAction(QStringLiteral("&About Pegged..."),
                                         this, &PeggedWindow::OnAbout);
    about->setMenuRole(QAction::AboutRole);

    // --- Status bar -------------------------------------------------------
    m_pegLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_pegLabel);

    // --- Restore preferred pattern from settings -------------------------
    QSettings cfg;
    int patInt = cfg.value(QStringLiteral("Pegged/Pattern"), int(PAT_CROSS)).toInt();
    if (patInt < 0 || patInt >= PAT_COUNT) patInt = PAT_CROSS;
    m_patternActions[patInt]->setChecked(true);
    m_board->SetPattern((PatternId)patInt);

    setMinimumSize(300, 340);
    resize(480, 520);
    UpdateStatus();
}

void PeggedWindow::UpdateStatus()
{
    int p = (int)m_board->GetPattern();
    statusBar()->showMessage(QStringLiteral("Pattern: %1")
                                 .arg(QString::fromUtf8(kPatternNames[p])));
    m_pegLabel->setText(QStringLiteral("Pegs remaining: %1")
                            .arg(m_board->PegsRemaining()));
    m_undoAction->setEnabled(m_board->CanUndo());
}

void PeggedWindow::OnNewGame() { m_board->NewGame(); UpdateStatus(); }
void PeggedWindow::OnUndo()    { m_board->Undo();    UpdateStatus(); }

void PeggedWindow::OnPattern(int id)
{
    if (id >= 0 && id < PAT_COUNT) {
        m_board->SetPattern((PatternId)id);
        UpdateStatus();
    }
}

void PeggedWindow::OnHowTo()
{
    QMessageBox::information(
        this, QStringLiteral("How to Play"),
        QStringLiteral(
            "Pegged is a peg-jumping solitaire game.\n\n"
            "Drag a peg over an adjacent peg and drop it into the empty\n"
            "hole on the far side. The jumped peg is removed.\n\n"
            "Jumps must be horizontal or vertical (no diagonals).\n\n"
            "Choose a starting pattern from the Options menu. The goal\n"
            "(on Solitaire) is to leave a single peg in the center."));
}

void PeggedWindow::OnAbout()
{
    QMessageBox::about(
        this, QStringLiteral("About Pegged"),
        QStringLiteral(
            "<b>Pegged</b> 1.0 (Qt port)<br><br>"
            "A portable peg-solitaire game.<br><br>"
            "(C) 1989-1990 Mike Blaylock<br>"
            "Qt port 2026"));
}

void PeggedWindow::closeEvent(QCloseEvent* evt)
{
    QSettings cfg;
    cfg.setValue(QStringLiteral("Pegged/Pattern"), int(m_board->GetPattern()));
    cfg.sync();
    evt->accept();
}

// ---------------------------------------------------------------------------
// PeggedBoard implementation
// ---------------------------------------------------------------------------

PeggedBoard::PeggedBoard(QWidget* parent)
    : QWidget(parent),
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
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(192, 192, 192));
    setPalette(pal);
    setMinimumSize(300, 300);
    std::memset(m_state, 0, sizeof(m_state));
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
    unsetCursor();
    update();
    emit stateChanged();
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
    update();
    emit stateChanged();
}

// ---------------------------------------------------------------------------
// Layout / hit-testing
// ---------------------------------------------------------------------------

void PeggedBoard::ComputeMetrics()
{
    const int w = width();
    const int h = height();
    // Keep the board square by using the smaller dimension.
    int s = qMin(w, h);
    if (s < 9) s = 9;
    m_xClient = m_yClient = s;
    m_xBlock = m_yBlock = s / (DIVISIONS + 2);
    if (m_xBlock < 1) m_xBlock = 1;
    if (m_yBlock < 1) m_yBlock = 1;
    m_xEdge    = qMax(1, m_xBlock / 4);
    m_yEdge    = qMax(1, m_yBlock / 4);
    m_xShadow  = qMax(1, m_xBlock / 10);
    m_yShadow  = qMax(1, m_yBlock / 10);
    m_winxEdge = (w - DIVISIONS * m_xBlock) / 2;
    m_winyEdge = (h - DIVISIONS * m_yBlock) / 2;
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

void PeggedBoard::DrawBoard(QPainter& dc)
{
    const QColor gray(192, 192, 192);
    const QColor dark(64, 64, 64);

    // Background
    dc.fillRect(0, 0, width(), height(), gray);

    // Shadow lines around the plus-shaped playing area (right and bottom).
    dc.setPen(QPen(dark, 2));
    // bottom edge of left arm
    dc.drawLine(m_winxEdge,                 m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 2 * m_xBlock,  m_winyEdge + 5 * m_yBlock + 1);
    // bottom edge of middle column
    dc.drawLine(m_winxEdge + 2 * m_xBlock,  m_winyEdge + 7 * m_yBlock + 1,
                m_winxEdge + 5 * m_xBlock,  m_winyEdge + 7 * m_yBlock + 1);
    // bottom edge of right arm
    dc.drawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1);
    // right edge of top arm
    dc.drawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge,
                m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 2 * m_yBlock);
    // right edge of middle row (continuation)
    dc.drawLine(m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 2 * m_yBlock,
                m_winxEdge + 7 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1);
    // right edge of bottom arm
    dc.drawLine(m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 5 * m_xBlock + 1, m_winyEdge + 7 * m_yBlock + 1);

    // Highlight lines (white) - top and left edges
    dc.setPen(QPen(Qt::white, 2));
    dc.drawLine(m_winxEdge - 1, m_winyEdge + 2 * m_yBlock,
                m_winxEdge - 1, m_winyEdge + 5 * m_yBlock - 1);
    dc.drawLine(m_winxEdge,     m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 2 * m_yBlock - 1);
    dc.drawLine(m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge);
    dc.drawLine(m_winxEdge + 2 * m_xBlock,     m_winyEdge - 1,
                m_winxEdge + 5 * m_xBlock - 1, m_winyEdge - 1);
    dc.drawLine(m_winxEdge + 5 * m_xBlock + 2, m_winyEdge + 2 * m_yBlock - 1,
                m_winxEdge + 7 * m_xBlock,     m_winyEdge + 2 * m_yBlock - 1);
    dc.drawLine(m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 5 * m_yBlock + 1,
                m_winxEdge + 2 * m_xBlock - 1, m_winyEdge + 7 * m_yBlock - 1);

    // Holes
    dc.setPen(QPen(Qt::black));
    dc.setBrush(Qt::NoBrush);
    for (int x = 0; x < DIVISIONS; ++x) {
        for (int y = 0; y < DIVISIONS; ++y) {
            if (!IsPlayable(x, y)) continue;
            int px = x * m_xBlock + m_winxEdge + m_xEdge;
            int py = y * m_yBlock + m_winyEdge + m_yEdge;
            int w  = m_xBlock - 2 * m_xEdge;
            int h  = m_yBlock - 2 * m_yEdge;
            dc.drawEllipse(px, py, w, h);
        }
    }
}

void PeggedBoard::DrawPegAtCell(QPainter& dc, int gx, int gy)
{
    int x = gx * m_xBlock + m_winxEdge;
    int y = gy * m_yBlock + m_winyEdge;
    int w = m_xBlock - 2 * m_xEdge;
    int h = m_yBlock - 2 * m_yEdge;

    // shadow
    dc.setPen(QPen(QColor(64, 64, 64), 1));
    dc.setBrush(QBrush(QColor(64, 64, 64)));
    dc.drawEllipse(x + m_xEdge + m_xShadow, y + m_yEdge + m_yShadow, w, h);

    // peg body
    dc.setPen(QPen(Qt::black));
    dc.setBrush(QBrush(QColor(0, 0, 255)));
    dc.drawEllipse(x + m_xEdge, y + m_yEdge, w, h);

    // highlight (offset inner ellipse, white)
    dc.setPen(QPen(Qt::white, 2));
    dc.setBrush(Qt::NoBrush);
    dc.drawEllipse(x + m_xEdge + m_xShadow, y + m_yEdge + m_yShadow,
                   w - 2 * m_xShadow, h - 2 * m_yShadow);
}

void PeggedBoard::DrawFloatingPeg(QPainter& dc, int cx, int cy)
{
    int w = m_xBlock - 2 * m_xEdge;
    int h = m_yBlock - 2 * m_yEdge;

    dc.setPen(QPen(Qt::black));
    dc.setBrush(QBrush(QColor(0, 0, 255)));
    dc.drawEllipse(cx - w / 2, cy - h / 2, w, h);

    dc.setPen(QPen(Qt::white, 2));
    dc.setBrush(Qt::NoBrush);
    dc.drawEllipse(cx - w / 2 + m_xShadow,
                   cy - h / 2 + m_yShadow,
                   w - 2 * m_xShadow, h - 2 * m_yShadow);
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void PeggedBoard::resizeEvent(QResizeEvent*)
{
    ComputeMetrics();
    update();
}

void PeggedBoard::paintEvent(QPaintEvent*)
{
    ComputeMetrics();
    QPainter dc(this);
    dc.setRenderHint(QPainter::Antialiasing, true);

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
        DrawFloatingPeg(dc, m_dragMouse.x(), m_dragMouse.y());
    }
}

void PeggedBoard::mousePressEvent(QMouseEvent* evt)
{
    if (evt->button() != Qt::LeftButton) { QWidget::mousePressEvent(evt); return; }

    const QPoint pos = evt->pos();
    int cx, cy;
    if (!CellFromPixel(pos.x(), pos.y(), cx, cy)) { return; }
    if (!m_state[cx][cy]) { return; }

    m_dragging = true;
    m_dragSrcX = cx;
    m_dragSrcY = cy;
    m_dragMouse = pos;
    setCursor(Qt::SizeAllCursor);
    update();
}

void PeggedBoard::mouseMoveEvent(QMouseEvent* evt)
{
    if (!m_dragging) return;
    m_dragMouse = evt->pos();
    update();
}

void PeggedBoard::mouseReleaseEvent(QMouseEvent* evt)
{
    if (!m_dragging) { QWidget::mouseReleaseEvent(evt); return; }

    m_dragging = false;
    unsetCursor();

    const QPoint pos = evt->pos();
    int cx, cy;
    if (CellFromPixel(pos.x(), pos.y(), cx, cy)) {
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

            update();
            emit stateChanged();

            if (!AnyMovesLeft()) {
                int remaining = PegsRemaining();
                QString msg = (remaining <= 1)
                    ? QStringLiteral("You Win!")
                    : QStringLiteral("Game Over.\n%1 peg%2 left.")
                          .arg(remaining)
                          .arg(remaining == 1 ? QLatin1String("")
                                              : QLatin1String("s"));
                QMessageBox::information(this, QStringLiteral("Pegged"), msg);
            }
            return;
        }
    }

    // invalid drop: snap peg back
    update();
}

#include "qtpegged.moc"

// ---------------------------------------------------------------------------
// App entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Pegged"));
    QApplication::setOrganizationName(QStringLiteral("Pegged"));
    QApplication::setApplicationDisplayName(QStringLiteral("Pegged"));

    PeggedWindow w;
    w.show();
    return app.exec();
}
