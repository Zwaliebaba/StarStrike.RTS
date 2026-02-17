#pragma once

#include "Canvas.h"

namespace Neuron
{
  class GuiWindow
  {
  public:
    GuiWindow(std::string _title, float _x, float _y, float _w, float _h);

    void SetVisible(bool _visible) { m_visible = _visible; }
    void SetDraggable(bool _draggable) { m_draggable = _draggable; }
    void SetResizable(bool _resizable) { m_resizable = _resizable; }

    void BeginWindow(Canvas& _canvas, BitmapFont& _font);
    void LabelRow(const std::string& _label, const std::string& _value);
    void DropdownRow(const std::string& _label, const std::string& _currentValue);
    void Separator();
    void TextLine(const std::string& _text, const XMFLOAT4& _color = {1.0f, 1.0f, 1.0f, 1.0f});
    void ProgressBar(float _fraction, const XMFLOAT4& _fillColor);
    void GraphLine(const std::vector<float>& _values, float _maxValue, const XMFLOAT4& _color);
    void EndWindow();

    bool HandleMouseDown(float _mx, float _my);
    void HandleMouseMove(float _mx, float _my);
    void HandleMouseUp();

  private:
    std::string m_title;
    float m_x, m_y, m_width, m_height;
    float m_cursorY = 0;
    bool m_visible = true;
    bool m_draggable = true;
    bool m_resizable = false;
    bool m_dragging = false;
    float m_dragOffsetX = 0, m_dragOffsetY = 0;

    Canvas* m_canvas = nullptr;
    BitmapFont* m_font = nullptr;

    static constexpr float PADDING_X = 8.0f;
    static constexpr float PADDING_Y = 4.0f;
  };
}
