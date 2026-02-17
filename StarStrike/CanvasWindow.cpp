#include "pch.h"
#include "CanvasWindow.h"

namespace Neuron
{
  CanvasWindow::CanvasWindow(std::string _title, float _x, float _y, float _w, float _h)
    : m_title(std::move(_title)), m_x(_x), m_y(_y), m_width(_w), m_height(_h)
  {
  }

  void CanvasWindow::BeginWindow(Canvas& _canvas, BitmapFont& _font)
  {
    m_canvas = &_canvas;
    m_font = &_font;

    if (!m_visible)
      return;

    float titleBarHeight = static_cast<float>(_font.GetLineHeight()) + 8.0f;

    // Window body background
    m_canvas->DrawRect(m_x, m_y, m_width, m_height, {0.08f, 0.08f, 0.12f, 0.9f});

    // Title bar background
    m_canvas->DrawRect(m_x, m_y, m_width, titleBarHeight, {0.15f, 0.15f, 0.2f, 0.95f});

    // Border outline
    m_canvas->DrawRectOutline(m_x, m_y, m_width, m_height, 1.0f, {0.4f, 0.5f, 0.6f, 0.8f});

    // Title text centered
    uint32_t titleWidth = _font.MeasureString(m_title);
    float titleX = m_x + (m_width - static_cast<float>(titleWidth)) * 0.5f;
    float titleY = m_y + 4.0f;
    m_canvas->DrawText(_font, titleX, titleY, m_title, {0.6f, 0.75f, 0.9f, 1.0f});

    // Content starts below title bar
    m_cursorY = m_y + titleBarHeight + PADDING_Y;
  }

  void CanvasWindow::LabelRow(const std::string& _label, const std::string& _value)
  {
    if (!m_visible || !m_canvas || !m_font) return;

    float rowHeight = static_cast<float>(m_font->GetLineHeight()) + PADDING_Y;
    float labelX = m_x + PADDING_X;

    // Label on the left (dim white)
    m_canvas->DrawText(*m_font, labelX, m_cursorY, _label, {0.7f, 0.7f, 0.7f, 1.0f});

    // Value on the right
    uint32_t valueWidth = m_font->MeasureString(_value);
    float valueX = m_x + m_width - PADDING_X - static_cast<float>(valueWidth);
    m_canvas->DrawText(*m_font, valueX, m_cursorY, _value, {1.0f, 1.0f, 1.0f, 1.0f});

    m_cursorY += rowHeight;
  }

  void CanvasWindow::DropdownRow(const std::string& _label, const std::string& _currentValue)
  {
    if (!m_visible || !m_canvas || !m_font) return;

    float rowHeight = static_cast<float>(m_font->GetLineHeight()) + PADDING_Y;
    float labelX = m_x + PADDING_X;

    m_canvas->DrawText(*m_font, labelX, m_cursorY, _label, {0.7f, 0.7f, 0.7f, 1.0f});

    // Value with dropdown indicator
    std::string display = _currentValue + " v";
    uint32_t displayWidth = m_font->MeasureString(display);
    float valueX = m_x + m_width - PADDING_X - static_cast<float>(displayWidth);
    m_canvas->DrawText(*m_font, valueX, m_cursorY, display, {0.9f, 0.9f, 1.0f, 1.0f});

    m_cursorY += rowHeight;
  }

  void CanvasWindow::Separator()
  {
    if (!m_visible || !m_canvas) return;

    m_cursorY += PADDING_Y * 0.5f;
    m_canvas->DrawRect(m_x + PADDING_X, m_cursorY, m_width - PADDING_X * 2.0f, 1.0f,
                       {0.3f, 0.35f, 0.4f, 0.6f});
    m_cursorY += 1.0f + PADDING_Y * 0.5f;
  }

  void CanvasWindow::TextLine(const std::string& _text, const XMFLOAT4& _color)
  {
    if (!m_visible || !m_canvas || !m_font) return;

    float rowHeight = static_cast<float>(m_font->GetLineHeight()) + PADDING_Y;
    m_canvas->DrawTextClipped(*m_font, m_x + PADDING_X, m_cursorY,
                              m_width - PADDING_X * 2.0f, _text, _color);
    m_cursorY += rowHeight;
  }

  void CanvasWindow::ProgressBar(float _fraction, const XMFLOAT4& _fillColor)
  {
    if (!m_visible || !m_canvas) return;

    float barX = m_x + PADDING_X;
    float barW = m_width - PADDING_X * 2.0f;
    float barH = 8.0f;

    // Background
    m_canvas->DrawRect(barX, m_cursorY, barW, barH, {0.15f, 0.15f, 0.2f, 0.8f});

    // Fill
    float clampedFraction = (std::max)(0.0f, (std::min)(1.0f, _fraction));
    if (clampedFraction > 0.0f)
      m_canvas->DrawRect(barX, m_cursorY, barW * clampedFraction, barH, _fillColor);

    m_cursorY += barH + PADDING_Y;
  }

  void CanvasWindow::GraphLine(const std::vector<float>& _values, float _maxValue, const XMFLOAT4& _color)
  {
    if (!m_visible || !m_canvas || _values.empty() || _maxValue <= 0.0f) return;

    float graphX = m_x + PADDING_X;
    float graphW = m_width - PADDING_X * 2.0f;
    float graphH = 40.0f;

    // Background
    m_canvas->DrawRect(graphX, m_cursorY, graphW, graphH, {0.1f, 0.1f, 0.15f, 0.8f});

    // Draw bars for each value
    float barWidth = graphW / static_cast<float>(_values.size());
    for (size_t i = 0; i < _values.size(); ++i)
    {
      float normalized = (std::min)(_values[i] / _maxValue, 1.0f);
      float barH = normalized * graphH;
      float barX2 = graphX + static_cast<float>(i) * barWidth;
      float barY = m_cursorY + graphH - barH;
      m_canvas->DrawRect(barX2, barY, (std::max)(barWidth - 1.0f, 1.0f), barH, _color);
    }

    m_cursorY += graphH + PADDING_Y;
  }

  void CanvasWindow::EndWindow()
  {
    m_canvas = nullptr;
    m_font = nullptr;
  }

  bool CanvasWindow::HandleMouseDown(float _mx, float _my)
  {
    if (!m_visible || !m_draggable) return false;

    float titleBarHeight = 18.0f; // approximate
    if (_mx >= m_x && _mx <= m_x + m_width &&
        _my >= m_y && _my <= m_y + titleBarHeight)
    {
      m_dragging = true;
      m_dragOffsetX = _mx - m_x;
      m_dragOffsetY = _my - m_y;
      return true;
    }
    return false;
  }

  void CanvasWindow::HandleMouseMove(float _mx, float _my)
  {
    if (m_dragging)
    {
      m_x = _mx - m_dragOffsetX;
      m_y = _my - m_dragOffsetY;
    }
  }

  void CanvasWindow::HandleMouseUp()
  {
    m_dragging = false;
  }
}
