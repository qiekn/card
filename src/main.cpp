#include <nlohmann/json.h>
#include <cmath>
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

  Vector2 textSize = MeasureTextEx(font, button.text, 20, 1);
  Vector2 textPos = {button.rect.x + (button.rect.width - textSize.x) / 2,
                     button.rect.y + (button.rect.height - textSize.y) / 2};

  DrawTextEx(font, button.text, textPos, 20, 1, textColor);
}

// 绘制锚点
void DrawAnchorPoint(AnchorPoint& anchor) {
  Color color = anchor.isHover ? RED : BLUE;
  if (anchor.isDragging) color = MAROON;  // 使用 MAROON 替代 DARKRED

  DrawCircleV(anchor.position, 8, color);
  DrawCircleV(anchor.position, 6, WHITE);
  DrawCircleLinesV(anchor.position, 8, BLACK);  // 使用 DrawCircleLinesV
}

int main() {
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(kCardWidth + 400, kCardHeight + 200, "Card Editor");
  SetTargetFPS(60);

  using json = nlohmann::json;

  // 加载纹理
  Texture2D texture = LoadTexture("assets/images/1.jpg");
  if (texture.id == 0) {
    std::cout << "Warning: Cannot load texture, using default texture" << std::endl;
    Image img = GenImageColor(400, 300, PURPLE);
    texture = LoadTextureFromImage(img);
    UnloadImage(img);
  }

  Font font = GetFontDefault();

  // 初始化变量
  float zoom = 1.0f;
  Vector2 offset = {0, 0};
  bool isDragging = false;
  Vector2 dragStart = {0, 0};
  Vector2 offsetStart = {0, 0};

  // 卡片显示区域
  Rectangle cardRect = {50, 100, (float)kCardWidth, (float)kCardHeight};

  // 扩展显示区域（用于显示超出部分）
  Rectangle extendedRect = {cardRect.x - 150, cardRect.y - 150, cardRect.width + 300,
                            cardRect.height + 300};

  // 创建按钮
  Button zoomInBtn = {{cardRect.x + cardRect.width + 20, 120}, "Zoom (+)", false};
  Button zoomOutBtn = {{cardRect.x + cardRect.width + 20, 160}, "Zoom (-)", false};
  Button saveBtn = {{cardRect.x + cardRect.width + 20, 220}, "Save", false};
  Button resetBtn = {{cardRect.x + cardRect.width + 20, 260}, "Reset", false};

  // 设置按钮大小
  zoomInBtn.rect.width = zoomOutBtn.rect.width = saveBtn.rect.width = resetBtn.rect.width = 100;
  zoomInBtn.rect.height = zoomOutBtn.rect.height = saveBtn.rect.height = resetBtn.rect.height = 35;

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
    }
  }

  while (!WindowShouldClose()) {
    Vector2 mousePos = GetMousePosition();
    bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    // 处理按钮交互
    zoomInBtn.isPressed = IsPointInRect(mousePos, zoomInBtn.rect) && mouseDown;
    zoomOutBtn.isPressed = IsPointInRect(mousePos, zoomOutBtn.rect) && mouseDown;
    saveBtn.isPressed = IsPointInRect(mousePos, saveBtn.rect) && mouseDown;
    resetBtn.isPressed = IsPointInRect(mousePos, resetBtn.rect) && mouseDown;

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
        saveConfig["cards"][0] = {{"s", zoom}, {"x", offset.x}, {"y", offset.y}};

        std::ofstream output("data/cards.json");
        if (output.is_open()) {
          output << saveConfig.dump(4);
          output.close();
          std::cout << "Card config saved" << std::endl;
        }
      } else if (IsPointInRect(mousePos, resetBtn.rect)) {
        zoom = 1.0f;
        offset = {0, 0};
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

    // 检查锚点悬停
    for (int i = 0; i < 4; i++) {
      float dist = CalculateDistance(mousePos, anchors[i].position);
      anchors[i].isHover = dist <= 12;
    }

    // 处理图片拖拽
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
        if (IsPointInRect(mousePos, zoomInBtn.rect) || IsPointInRect(mousePos, zoomOutBtn.rect) ||
            IsPointInRect(mousePos, saveBtn.rect) || IsPointInRect(mousePos, resetBtn.rect)) {
          onButton = true;
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
    }
    if (IsKeyPressed(KEY_S) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
      // 保存
      json saveConfig;
      saveConfig["cards"] = json::array();
      saveConfig["cards"][0] = {{"s", zoom}, {"x", offset.x}, {"y", offset.y}};

      std::ofstream output("data/cards.json");
      if (output.is_open()) {
        output << saveConfig.dump(4);
        output.close();
      }
    }

    // 绘制
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // 计算图片的源矩形（不重复，只显示图片的一部分）
    Rectangle sourceRec = {
        0,  // 始终从图片的 (0,0) 开始
        0,
        (float)texture.width,  // 使用完整的图片宽度
        (float)texture.height  // 使用完整的图片高度
    };

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
    DrawButton(zoomInBtn, font);
    DrawButton(zoomOutBtn, font);
    DrawButton(saveBtn, font);
    DrawButton(resetBtn, font);

    // 绘制信息文本
    DrawText("Card Editor", 10, 10, 24, DARKGRAY);
    DrawText(TextFormat("Scale: %.2f", zoom), 10, 40, 20, DARKGRAY);
    DrawText(TextFormat("Offset: (%.0f, %.0f)", offset.x, offset.y), 10, 65, 20, DARKGRAY);

    // 绘制操作提示
    DrawText("Controls:", cardRect.x + cardRect.width + 20, 320, 18, DARKGRAY);
    DrawText("• Drag to move image", cardRect.x + cardRect.width + 20, 345, 14, GRAY);
    DrawText("• +/- keys to scale", cardRect.x + cardRect.width + 20, 365, 14, GRAY);
    DrawText("• R key to reset", cardRect.x + cardRect.width + 20, 385, 14, GRAY);
    DrawText("• Ctrl+S to save", cardRect.x + cardRect.width + 20, 405, 14, GRAY);

    if (isDragging) {
      DrawText("Dragging...", cardRect.x, cardRect.y - 25, 16, RED);
    }

    EndDrawing();
  }

  UnloadTexture(texture);
  CloseWindow();
  return 0;
}
