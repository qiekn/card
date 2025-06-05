#include <nlohmann/json.h>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include "constants.h"
#include "raylib.h"

// 按钮结构
struct Button {
  Rectangle rect;
  const char* text;
  bool isPressed;
};

// 锚点结构
struct AnchorPoint {
  Vector2 position;
  bool isHover;
  bool isDragging;
};

// 检查点是否在矩形内
bool IsPointInRect(Vector2 point, Rectangle rect) {
  return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y &&
         point.y <= rect.y + rect.height;
}

// 计算两点之间的距离
float CalculateDistance(Vector2 point1, Vector2 point2) {
  float dx = point1.x - point2.x;
  float dy = point1.y - point2.y;
  return sqrt(dx * dx + dy * dy);
}

// 绘制按钮
void DrawButton(Button& button, Font font) {
  Color buttonColor = button.isPressed ? DARKGRAY : LIGHTGRAY;
  Color textColor = button.isPressed ? WHITE : BLACK;

  DrawRectangleRec(button.rect, buttonColor);
  DrawRectangleLinesEx(button.rect, 2, GRAY);

  Vector2 textSize = MeasureTextEx(font, button.text, 18, 1);
  Vector2 textPos = {button.rect.x + (button.rect.width - textSize.x) / 2,
                     button.rect.y + (button.rect.height - textSize.y) / 2};

  DrawTextEx(font, button.text, textPos, 18, 1, textColor);
}

// 绘制锚点
void DrawAnchorPoint(AnchorPoint& anchor) {
  Color color = anchor.isHover ? RED : BLUE;
  if (anchor.isDragging) color = MAROON;

  DrawCircleV(anchor.position, 8, color);
  DrawCircleV(anchor.position, 6, WHITE);
  DrawCircleLinesV(anchor.position, 8, BLACK);
}

int main() {
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT);  // 启用 4x 多重采样抗锯齿
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(kCardWidth + 450, kCardHeight + 200, "Card Editor");
  SetTargetFPS(60);

  using json = nlohmann::json;

  // 加载 Noto Serif 字体
  Font notoRegular = LoadFontEx("assets/fonts/noto-regular.ttf", 64, NULL, 0);
  Font notoItalic = LoadFontEx("assets/fonts/noto-italic.ttf", 64, NULL, 0);

  // 加载纹理
  Texture2D texture = LoadTexture("assets/images/1.jpg");
  if (texture.id == 0) {
    std::cout << "Warning: Cannot load texture, using default texture" << std::endl;
    Image img = GenImageColor(400, 300, PURPLE);
    texture = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  // 初始化变量
  float zoom = 1.0f;
  Vector2 offset = {0, 0};
  bool isDragging = false;
  Vector2 dragStart = {0, 0};
  Vector2 offsetStart = {0, 0};

  // 锚点拖拽相关
  int draggingAnchor = -1;  // -1表示没有拖拽锚点
  float initialZoom = 1.0f;
  Vector2 initialMousePos = {0, 0};
  Vector2 scaleCenter = {0, 0};  // 缩放中心点

  // 翻转状态
  bool flipHorizontal = false;
  bool flipVertical = false;

  // 卡片显示区域
  Rectangle cardRect = {50, 100, (float)kCardWidth, (float)kCardHeight};

  // 扩展显示区域（用于显示超出部分）
  Rectangle extendedRect = {cardRect.x - 150, cardRect.y - 150, cardRect.width + 300,
                            cardRect.height + 300};

  // 创建按钮 - 重新排列
  float btnX = cardRect.x + cardRect.width + 20;
  float btnY = 120;
  float btnW = 100;
  float btnH = 30;
  float btnSpacing = 35;

  // 居中按钮
  Button centerHBtn = {{btnX, btnY + btnSpacing * 0}, "Center H", false};
  Button centerVBtn = {{btnX, btnY + btnSpacing * 1}, "Center V", false};

  // 缩放按钮
  Button zoomInBtn = {{btnX, btnY + btnSpacing * 2}, "Zoom (+)", false};
  Button zoomOutBtn = {{btnX, btnY + btnSpacing * 3}, "Zoom (-)", false};

  // 翻转按钮
  Button flipHBtn = {{btnX, btnY + btnSpacing * 4}, "Flip H", false};
  Button flipVBtn = {{btnX, btnY + btnSpacing * 5}, "Flip V", false};

  // 保存重置按钮
  Button saveBtn = {{btnX, btnY + btnSpacing * 6}, "Save", false};
  Button resetBtn = {{btnX, btnY + btnSpacing * 7}, "Reset", false};

  // 设置按钮大小
  Button* buttons[] = {&zoomInBtn, &zoomOutBtn, &saveBtn,    &resetBtn,
                       &flipHBtn,  &flipVBtn,   &centerHBtn, &centerVBtn};

  for (int i = 0; i < 8; i++) {
    buttons[i]->rect.width = btnW;
    buttons[i]->rect.height = btnH;
  }

  // 锚点
  AnchorPoint anchors[4];

  // 从配置文件加载
  json config;
  std::ifstream input("data/cards.json");
  if (input.is_open()) {
    input >> config;
    input.close();
    if (config.contains("cards") && config["cards"].size() > 0) {
      auto card = config["cards"][0];
      if (card.contains("s")) zoom = card["s"];
      if (card.contains("x")) offset.x = card["x"];
      if (card.contains("y")) offset.y = card["y"];
      if (card.contains("flipH")) flipHorizontal = card["flipH"];
      if (card.contains("flipV")) flipVertical = card["flipV"];
    }
  }

  while (!WindowShouldClose()) {
    Vector2 mousePos = GetMousePosition();
    bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    // 处理按钮交互
    for (int i = 0; i < 8; i++) {
      buttons[i]->isPressed = IsPointInRect(mousePos, buttons[i]->rect) && mouseDown;
    }

    if (mousePressed) {
      if (IsPointInRect(mousePos, zoomInBtn.rect)) {
        zoom += 0.1f;
        if (zoom > 5.0f) zoom = 5.0f;
      } else if (IsPointInRect(mousePos, zoomOutBtn.rect)) {
        zoom -= 0.1f;
        if (zoom < 0.1f) zoom = 0.1f;
      } else if (IsPointInRect(mousePos, saveBtn.rect)) {
        // 保存配置
        json saveConfig;
        saveConfig["cards"] = json::array();
        saveConfig["cards"][0] = {{"s", zoom},
                                  {"x", offset.x},
                                  {"y", offset.y},
                                  {"flipH", flipHorizontal},
                                  {"flipV", flipVertical}};

        std::ofstream output("data/cards.json");
        if (output.is_open()) {
          output << saveConfig.dump(4);
          output.close();
          std::cout << "Card config saved" << std::endl;
        }
      } else if (IsPointInRect(mousePos, resetBtn.rect)) {
        zoom = 1.0f;
        offset = {0, 0};
        flipHorizontal = false;
        flipVertical = false;
      } else if (IsPointInRect(mousePos, flipHBtn.rect)) {
        flipHorizontal = !flipHorizontal;
      } else if (IsPointInRect(mousePos, flipVBtn.rect)) {
        flipVertical = !flipVertical;
      } else if (IsPointInRect(mousePos, centerHBtn.rect)) {
        // 水平居中：调整缩放使图片宽度与卡片宽度对齐，然后居中
        float targetZoom = (float)kCardWidth / texture.width;
        zoom = targetZoom;
        offset.x = 0;  // 水平居中
      } else if (IsPointInRect(mousePos, centerVBtn.rect)) {
        // 垂直居中：调整缩放使图片高度与卡片高度对齐，然后居中
        float targetZoom = (float)kCardHeight / texture.height;
        zoom = targetZoom;
        offset.y = 0;  // 垂直居中
      }
    }

    // 计算图片在屏幕上的实际显示区域（基于zoom和offset）
    float imgDisplayWidth = texture.width * zoom;
    float imgDisplayHeight = texture.height * zoom;

    // 图片左上角在屏幕上的位置
    float imgScreenX = cardRect.x - offset.x * zoom;
    float imgScreenY = cardRect.y - offset.y * zoom;

    // 更新锚点位置（跟随图片的四个角）
    anchors[0].position = {imgScreenX, imgScreenY};                                       // 左上
    anchors[1].position = {imgScreenX + imgDisplayWidth, imgScreenY};                     // 右上
    anchors[2].position = {imgScreenX + imgDisplayWidth, imgScreenY + imgDisplayHeight};  // 右下
    anchors[3].position = {imgScreenX, imgScreenY + imgDisplayHeight};                    // 左下

    // 检查锚点悬停和拖拽
    for (int i = 0; i < 4; i++) {
      float dist = CalculateDistance(mousePos, anchors[i].position);
      anchors[i].isHover = dist <= 12;

      if (draggingAnchor == -1 && mousePressed && anchors[i].isHover) {
        // 开始拖拽锚点
        draggingAnchor = i;
        anchors[i].isDragging = true;
        initialZoom = zoom;
        initialMousePos = mousePos;

        // 计算缩放中心点（对角锚点）
        int oppositeCorner = (i + 2) % 4;
        scaleCenter = anchors[oppositeCorner].position;
      }
    }

    // 处理锚点拖拽缩放
    if (draggingAnchor != -1) {
      if (mouseDown) {
        // 计算从缩放中心到当前鼠标位置的距离
        float currentDist = CalculateDistance(mousePos, scaleCenter);
        float initialDist = CalculateDistance(initialMousePos, scaleCenter);

        if (initialDist > 0) {
          // 按比例缩放
          float scaleRatio = currentDist / initialDist;
          zoom = initialZoom * scaleRatio;

          // 限制缩放范围
          if (zoom < 0.1f) zoom = 0.1f;
          if (zoom > 5.0f) zoom = 5.0f;
        }
      } else {
        // 停止拖拽
        anchors[draggingAnchor].isDragging = false;
        draggingAnchor = -1;
      }
    }

    // 处理图片拖拽（只有在没有拖拽锚点时才能拖拽图片）
    if (draggingAnchor == -1) {
      if (!isDragging) {
        // 检查是否开始拖拽（在扩展区域内且不在按钮或锚点上）
        if (mousePressed && IsPointInRect(mousePos, extendedRect)) {
          bool onAnchor = false;
          bool onButton = false;

          // 检查是否在锚点上
          for (int i = 0; i < 4; i++) {
            if (anchors[i].isHover) {
              onAnchor = true;
              break;
            }
          }

          // 检查是否在按钮上
          for (int i = 0; i < 8; i++) {
            if (IsPointInRect(mousePos, buttons[i]->rect)) {
              onButton = true;
              break;
            }
          }

          if (!onAnchor && !onButton) {
            isDragging = true;
            dragStart = mousePos;
            offsetStart = offset;
          }
        }
      } else {
        if (mouseDown) {
          Vector2 delta = {mousePos.x - dragStart.x, mousePos.y - dragStart.y};
          offset.x = offsetStart.x - delta.x / zoom;
          offset.y = offsetStart.y - delta.y / zoom;
        } else {
          isDragging = false;
        }
      }
    }

    // 键盘快捷键
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
      zoom += 0.1f;
      if (zoom > 5.0f) zoom = 5.0f;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
      zoom -= 0.1f;
      if (zoom < 0.1f) zoom = 0.1f;
    }
    if (IsKeyPressed(KEY_R)) {
      zoom = 1.0f;
      offset = {0, 0};
      flipHorizontal = false;
      flipVertical = false;
    }
    if (IsKeyPressed(KEY_H)) {
      flipHorizontal = !flipHorizontal;
    }
    if (IsKeyPressed(KEY_V)) {
      flipVertical = !flipVertical;
    }
    if (IsKeyPressed(KEY_S) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
      // 保存
      json saveConfig;
      saveConfig["cards"] = json::array();
      saveConfig["cards"][0] = {{"s", zoom},
                                {"x", offset.x},
                                {"y", offset.y},
                                {"flipH", flipHorizontal},
                                {"flipV", flipVertical}};

      std::ofstream output("data/cards.json");
      if (output.is_open()) {
        output << saveConfig.dump(4);
        output.close();
      }
    }

    // 绘制
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // 计算图片的源矩形（处理翻转）
    Rectangle sourceRec = {flipHorizontal ? (float)texture.width : 0,
                           flipVertical ? (float)texture.height : 0,
                           flipHorizontal ? -(float)texture.width : (float)texture.width,
                           flipVertical ? -(float)texture.height : (float)texture.height};

    // 计算图片在屏幕上的目标矩形
    Rectangle destRec = {imgScreenX, imgScreenY, imgDisplayWidth, imgDisplayHeight};

    // 首先在扩展区域绘制超出部分（半透明）
    BeginScissorMode(extendedRect.x, extendedRect.y, extendedRect.width, extendedRect.height);
    DrawTexturePro(texture, sourceRec, destRec, {0, 0}, 0.0f, ColorAlpha(WHITE, 0.4f));
    EndScissorMode();

    // 然后在卡片区域绘制正常部分
    BeginScissorMode(cardRect.x, cardRect.y, cardRect.width, cardRect.height);
    DrawTexturePro(texture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
    EndScissorMode();

    // 绘制卡片边框
    DrawRectangleLinesEx(cardRect, 3, BLACK);

    // 绘制锚点（只有在扩展区域内才绘制）
    for (int i = 0; i < 4; i++) {
      if (IsPointInRect(anchors[i].position, extendedRect)) {
        DrawAnchorPoint(anchors[i]);
      }
    }

    // 绘制按钮
    for (int i = 0; i < 8; i++) {
      DrawButton(*buttons[i], notoRegular);
    }

    // clang-format off
    // 绘制信息文本
    DrawTextEx(notoRegular, "Card Editor", (Vector2){10, 10}, 24, 1, DARKGRAY);
    DrawTextEx(notoRegular, TextFormat("Scale: %.2f", zoom), (Vector2){10, 40}, 16, 1, DARKGRAY);
    DrawTextEx(notoRegular, TextFormat("Offset: (%.0f, %.0f)", offset.x, offset.y), (Vector2){10, 60}, 16, 1, DARKGRAY);
    DrawTextEx(notoRegular,
        TextFormat("Flip h: %s, v: %s", flipHorizontal ? "on" : "off", flipVertical ? "on" : "off"),
        (Vector2){10, 80}, 16, 1, DARKGRAY);

    // 使用 Noto Serif Italic 字体绘制操作提示
    DrawTextEx(notoItalic, "Controls:", (Vector2){btnX, btnY + btnSpacing * 8 + 20}, 18, 1, DARKGRAY);
    DrawTextEx(notoItalic, " Drag image to move", (Vector2){btnX, btnY + btnSpacing * 8 + 45}, 14, 1, GRAY);
    DrawTextEx(notoItalic, " Drag anchors to scale", (Vector2){btnX, btnY + btnSpacing * 8 + 65}, 14, 1, GRAY);
    DrawTextEx(notoItalic, " +/- keys to scale", (Vector2){btnX, btnY + btnSpacing * 8 + 85}, 14, 1, GRAY);
    DrawTextEx(notoItalic, " H/V keys to flip", (Vector2){btnX, btnY + btnSpacing * 8 + 105}, 14, 1, GRAY);
    DrawTextEx(notoItalic, " R key to reset", (Vector2){btnX, btnY + btnSpacing * 8 + 125}, 14, 1, GRAY);
    DrawTextEx(notoItalic, " Ctrl+S to save", (Vector2){btnX, btnY + btnSpacing * 8 + 145}, 14, 1, GRAY);

    if (isDragging) {
      DrawTextEx(notoRegular, "Moving image...", (Vector2){cardRect.x, cardRect.y - 25}, 16, 1, RED);
    }
    if (draggingAnchor != -1) {
      DrawTextEx(notoRegular, "Scaling...", (Vector2){cardRect.x, cardRect.y - 25}, 16, 1, BLUE);
    }
    // clang-format on

    EndDrawing();
  }

  UnloadTexture(texture);
  CloseWindow();
  return 0;
}
