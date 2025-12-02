// 谷歌小恐龙游戏 - 障碍物刷新优化版 (C语言风格 + EasyX)
#include <graphics.h>
#include <conio.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <windows.h>

// 游戏常量定义
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 400
#define GROUND_HEIGHT 350
#define DINO_WIDTH 40
#define DINO_HEIGHT 60
#define CACTUS_WIDTH 20
#define CACTUS_HEIGHT 40
#define BIRD_WIDTH 30
#define BIRD_HEIGHT 20
#define GRAVITY 1.2
#define JUMP_STRENGTH 18
#define GAME_SPEED 8
#define TARGET_FPS 60
#define MIN_OBSTACLE_DISTANCE 300  // 最小障碍物间距
#define MAX_OBSTACLE_DISTANCE 600  // 新增：最大障碍物间距
#define MIN_SPAWN_INTERVAL 25      // 新增：最小生成间隔
#define MAX_SPAWN_INTERVAL 60      // 新增：最大生成间隔
#define MAX_OBSTACLES_ON_SCREEN 2  // 屏幕上最大障碍物数量

// 游戏状态枚举
typedef enum {
    MENU,
    PLAYING,
    GAME_OVER
} GameState;

// 恐龙结构体
typedef struct {
    int x, y;
    int width, height;
    int velocityY;
    int isJumping;
    int isDucking;
    int frame;
} Dino;

// 障碍物结构体
typedef struct {
    int x, y;
    int width, height;
    int type; // 0:仙人掌小, 1:仙人掌大, 2:鸟
    int passed;
} Obstacle;

// 云朵结构体
typedef struct {
    int x, y;
    int speed;
} Cloud;

// 游戏全局变量
Dino dino;
Obstacle obstacles[5];
Cloud clouds[3];
int obstacleCount = 0;
int score = 0;
int highScore = 0;
GameState gameState = MENU;
int gameSpeed = GAME_SPEED;
int nightMode = 0;
int cloudCount = 3;
int frameCount = 0;
int framesSinceLastObstacle = 0;
int nextSpawnInterval = 35; // 当前生成间隔（随机变化）
int nextMinDistance = 350;  // 当前最小距离（随机变化）

// 函数声明
void initGame();
void initGraphics();
void handleInput();
void updateGame();
void renderGame();
void drawMenu();
void drawPlayingState();
void drawGameOver();
void drawDino();
void updateDino();
void drawGround();
void generateObstacle();
void updateObstacles();
void drawObstacles();
void drawClouds();
void updateClouds();
void checkCollision();
void drawScore();
void drawNightSky();

// 初始化游戏
void initGame() {
    // 初始化恐龙
    dino.x = 100;
    dino.y = GROUND_HEIGHT - DINO_HEIGHT;
    dino.width = DINO_WIDTH;
    dino.height = DINO_HEIGHT;
    dino.velocityY = 0;
    dino.isJumping = 0;
    dino.isDucking = 0;
    dino.frame = 0;
    
    // 初始化障碍物
    obstacleCount = 0;
    for (int i = 0; i < 5; i++) {
        obstacles[i].x = 0;
        obstacles[i].passed = 1;
    }
    
    // 初始化云朵
    for (int i = 0; i < cloudCount; i++) {
        clouds[i].x = rand() % SCREEN_WIDTH;
        clouds[i].y = 50 + rand() % 100;
        clouds[i].speed = 1 + rand() % 3;
    }
    
    // 重置分数和游戏速度
    score = 0;
    gameSpeed = GAME_SPEED;
    frameCount = 0;
    framesSinceLastObstacle = 0;
    
    // 随机设置初始生成间隔和最小距离
    nextSpawnInterval = MIN_SPAWN_INTERVAL + rand() % (MAX_SPAWN_INTERVAL - MIN_SPAWN_INTERVAL);
    nextMinDistance = MIN_OBSTACLE_DISTANCE + rand() % (MAX_OBSTACLE_DISTANCE - MIN_OBSTACLE_DISTANCE);
    
    // 随机决定是否为夜晚模式
    nightMode = rand() % 4 == 0;
    
    // 游戏开始时立即尝试生成第一个障碍物
    framesSinceLastObstacle = nextSpawnInterval;
}

// 初始化图形
void initGraphics() {
    initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // 开启双缓冲，解决闪烁问题
    BeginBatchDraw();
    
    // 设置随机种子
    srand((unsigned int)time(NULL));
}

// 处理输入 - 使用EasyX消息系统
void handleInput() {
    ExMessage msg;
    
    // 检查所有输入消息
    while (peekmessage(&msg, EX_KEY)) {
        // 只处理按键按下消息
        if (msg.message == WM_KEYDOWN) {
            int key = msg.vkcode;
            
            if (gameState == MENU) {
                if (key == VK_SPACE) {
                    gameState = PLAYING;
                    initGame();
                    // 清空输入缓冲区
                    flushmessage(EX_KEY);
                    return;
                } else if (key == VK_ESCAPE) {
                    EndBatchDraw();
                    closegraph();
                    exit(0);
                }
            } 
            else if (gameState == PLAYING) {
                if (key == VK_SPACE && !dino.isJumping) {
                    dino.velocityY = -JUMP_STRENGTH;
                    dino.isJumping = 1;
                } else if (key == VK_ESCAPE) {
                    gameState = MENU;
                } else if (key == VK_DOWN) {
                    dino.isDucking = 1;
                }
            } 
            else if (gameState == GAME_OVER) {
                if (key == VK_SPACE) {
                    gameState = PLAYING;
                    initGame();
                    flushmessage(EX_KEY);
                    return;
                } else if (key == VK_ESCAPE) {
                    gameState = MENU;
                }
            }
        }
        // 处理按键释放
        else if (msg.message == WM_KEYUP) {
            if (msg.vkcode == VK_DOWN) {
                dino.isDucking = 0;
            }
        }
    }
    
    // 额外检查持续按键（确保下蹲响应）
    if (gameState == PLAYING) {
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            dino.isDucking = 1;
        } else {
            dino.isDucking = 0;
        }
    }
}

// 更新游戏逻辑
void updateGame() {
    if (gameState == PLAYING) {
        updateDino();
        updateObstacles();
        updateClouds();
        checkCollision();
        frameCount++;
        framesSinceLastObstacle++; // 每帧增加障碍物生成计时器
    }
}

// 渲染游戏
void renderGame() {
    // 清空屏幕
    cleardevice();
    
    // 根据游戏状态渲染不同界面
    switch (gameState) {
        case MENU:
            drawMenu();
            break;
        case PLAYING:
            drawPlayingState();
            break;
        case GAME_OVER:
            drawGameOver();
            break;
    }
    
    // 双缓冲：一次性绘制到屏幕
    FlushBatchDraw();
}

// 绘制菜单界面
void drawMenu() {
    // 绘制背景
    setfillcolor(RGB(30, 30, 50));
    solidrectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // 绘制星空背景
    setfillcolor(RGB(255, 255, 255));
    for (int i = 0; i < 50; i++) {
        int x = rand() % SCREEN_WIDTH;
        int y = rand() % SCREEN_HEIGHT;
        if (x % 3 == 0) {
            solidcircle(x, y, 1);
        }
    }
    
    // 标题
    settextcolor(RGB(100, 255, 100));
    settextstyle(50, 0, _T("Consolas"));
    outtextxy(SCREEN_WIDTH / 2 - 180, SCREEN_HEIGHT / 2 - 120, _T("谷歌小恐龙"));
    
    // 作者信息
    settextcolor(RGB(200, 200, 255));
    settextstyle(20, 0, _T("Consolas"));
    outtextxy(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 - 60, _T("使用C语言风格和EasyX实现"));
    
    // 游戏说明
    settextcolor(RGB(255, 255, 200));
    settextstyle(18, 0, _T("Consolas"));
    outtextxy(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2, _T("游戏控制:"));
    outtextxy(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 30, _T("空格键 - 跳跃"));
    outtextxy(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 60, _T("下箭头 - 下蹲"));
    outtextxy(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 90, _T("ESC - 返回菜单/退出"));
    
    // 最高分显示
    char highScoreText[50];
    sprintf(highScoreText, "最高分: %d", highScore);
    settextcolor(RGB(255, 255, 100));
    settextstyle(25, 0, _T("Consolas"));
    outtextxy(SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 + 130, highScoreText);
    
    // 开始游戏提示（闪烁效果）
    if (frameCount % 40 < 20) {
        settextcolor(RGB(100, 255, 100));
        settextstyle(25, 0, _T("Consolas"));
        outtextxy(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 180, _T("按空格键开始游戏"));
    }
    
    // 绘制示例恐龙
    setfillcolor(RGB(80, 180, 80));
    fillrectangle(SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2 - 200, 
                  SCREEN_WIDTH / 2 + 20, SCREEN_HEIGHT / 2 - 140);
    fillrectangle(SCREEN_WIDTH / 2 - 10, SCREEN_HEIGHT / 2 - 215, 
                  SCREEN_WIDTH / 2 + 10, SCREEN_HEIGHT / 2 - 185);
    setfillcolor(RGB(255, 255, 255));
    solidcircle(SCREEN_WIDTH / 2 - 5, SCREEN_HEIGHT / 2 - 205, 3);
}

// 绘制游戏进行状态
void drawPlayingState() {
    // 绘制背景
    if (nightMode) {
        // 夜晚背景
        setfillcolor(RGB(20, 20, 40));
        solidrectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        drawNightSky();
    } else {
        // 白天背景
        setfillcolor(RGB(180, 230, 255));
        solidrectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    
    // 绘制游戏元素
    drawClouds();
    drawGround();
    drawObstacles();
    drawDino();
    drawScore();
}

// 绘制游戏结束界面
void drawGameOver() {
    // 先绘制游戏场景作为背景
    drawPlayingState();
    
    // 半透明黑色覆盖层（模拟效果）
    setfillcolor(RGB(0, 0, 0));
    for (int i = 0; i < 2; i++) {
        // 绘制两次模拟半透明效果
        solidrectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    
    // 游戏结束面板
    setfillcolor(RGB(40, 40, 70));
    int panelWidth = 400;
    int panelHeight = 250;
    int panelX = (SCREEN_WIDTH - panelWidth) / 2;
    int panelY = (SCREEN_HEIGHT - panelHeight) / 2;
    
    fillrectangle(panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    
    // 面板边框
    setlinecolor(RGB(100, 100, 150));
    rectangle(panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    rectangle(panelX + 5, panelY + 5, panelX + panelWidth - 5, panelY + panelHeight - 5);
    
    // 游戏结束文本
    settextcolor(RGB(255, 100, 100));
    settextstyle(40, 0, _T("Consolas"));
    outtextxy(panelX + 100, panelY + 30, _T("游戏结束"));
    
    // 分数显示
    char scoreText[50];
    sprintf(scoreText, "分数: %d", score);
    settextcolor(RGB(255, 255, 200));
    settextstyle(25, 0, _T("Consolas"));
    outtextxy(panelX + 120, panelY + 90, scoreText);
    
    // 最高分显示
    if (score > highScore) {
        highScore = score;
        settextcolor(RGB(255, 255, 100));
        outtextxy(panelX + 80, panelY + 130, _T("新纪录!"));
    }
    
    sprintf(scoreText, "最高分: %d", highScore);
    settextcolor(RGB(200, 200, 255));
    outtextxy(panelX + 100, panelY + 160, scoreText);
    
    // 重新开始提示
    settextcolor(RGB(200, 255, 200));
    settextstyle(20, 0, _T("Consolas"));
    outtextxy(panelX + 60, panelY + 200, _T("按空格键重新开始"));
    outtextxy(panelX + 80, panelY + 225, _T("按ESC键返回菜单"));
}

// 绘制夜晚星空
void drawNightSky() {
    setfillcolor(RGB(255, 255, 255));
    for (int i = 0; i < 100; i++) {
        int x = (rand() + frameCount) % SCREEN_WIDTH;
        int y = (rand() + frameCount / 2) % (GROUND_HEIGHT - 100);
        int size = rand() % 3 + 1;
        if (x % 4 == 0) {
            solidcircle(x, y, size);
        }
    }
}

// 绘制恐龙
void drawDino() {
    // 根据状态设置颜色
    COLORREF dinoColor = nightMode ? RGB(100, 180, 100) : RGB(80, 180, 80);
    COLORREF eyeColor = RGB(255, 255, 255);
    COLORREF pupilColor = RGB(0, 0, 0);
    
    // 绘制恐龙身体
    setfillcolor(dinoColor);
    if (dino.isDucking) {
        // 下蹲状态
        fillrectangle(dino.x, dino.y + 20, dino.x + dino.width, dino.y + dino.height);
        
        // 绘制恐龙头（下蹲）
        fillrectangle(dino.x + 10, dino.y + 10, dino.x + dino.width - 10, dino.y + 40);
        
        // 绘制眼睛
        setfillcolor(eyeColor);
        solidcircle(dino.x + 25, dino.y + 20, 5);
        setfillcolor(pupilColor);
        solidcircle(dino.x + 26, dino.y + 20, 2);
        
        // 绘制脚（跑步动画）
        setfillcolor(dinoColor);
        if (frameCount % 20 < 10) {
            fillrectangle(dino.x + 5, dino.y + dino.height - 10, 
                         dino.x + 15, dino.y + dino.height);
            fillrectangle(dino.x + 25, dino.y + dino.height - 5, 
                         dino.x + 35, dino.y + dino.height);
        } else {
            fillrectangle(dino.x + 5, dino.y + dino.height - 5, 
                         dino.x + 15, dino.y + dino.height);
            fillrectangle(dino.x + 25, dino.y + dino.height - 10, 
                         dino.x + 35, dino.y + dino.height);
        }
    } else {
        // 站立/跳跃状态
        fillrectangle(dino.x, dino.y, dino.x + dino.width, dino.y + dino.height);
        
        // 绘制恐龙头
        fillrectangle(dino.x + 10, dino.y - 15, dino.x + dino.width - 10, dino.y + 10);
        
        // 绘制眼睛
        setfillcolor(eyeColor);
        solidcircle(dino.x + 25, dino.y - 5, 5);
        setfillcolor(pupilColor);
        solidcircle(dino.x + 26, dino.y - 5, 2);
        
        // 绘制嘴
        setlinecolor(RGB(0, 0, 0));
        line(dino.x + 30, dino.y, dino.x + 35, dino.y - 5);
        
        // 绘制脚（仅当在地面上时）
        if (!dino.isJumping) {
            setfillcolor(dinoColor);
            if (frameCount % 20 < 10) {
                fillrectangle(dino.x + 5, dino.y + dino.height - 10, 
                             dino.x + 15, dino.y + dino.height);
                fillrectangle(dino.x + 25, dino.y + dino.height - 5, 
                             dino.x + 35, dino.y + dino.height);
            } else {
                fillrectangle(dino.x + 5, dino.y + dino.height - 5, 
                             dino.x + 15, dino.y + dino.height);
                fillrectangle(dino.x + 25, dino.y + dino.height - 10, 
                             dino.x + 35, dino.y + dino.height);
            }
        }
    }
}

// 更新恐龙状态
void updateDino() {
    // 应用重力
    dino.velocityY += GRAVITY;
    dino.y += dino.velocityY;
    
    // 检测地面碰撞
    if (dino.y >= GROUND_HEIGHT - dino.height) {
        dino.y = GROUND_HEIGHT - dino.height;
        dino.velocityY = 0;
        dino.isJumping = 0;
    }
    
    // 下蹲时调整高度
    if (dino.isDucking) {
        dino.height = DINO_HEIGHT - 20;
        dino.y = GROUND_HEIGHT - dino.height;
    } else {
        dino.height = DINO_HEIGHT;
    }
}

// 绘制地面
void drawGround() {
    // 绘制地面
    setfillcolor(nightMode ? RGB(60, 60, 70) : RGB(220, 200, 170));
    solidrectangle(0, GROUND_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // 绘制地面纹理
    setlinecolor(nightMode ? RGB(80, 80, 90) : RGB(200, 180, 150));
    for (int i = -(frameCount * gameSpeed) % 20; i < SCREEN_WIDTH; i += 20) {
        line(i, GROUND_HEIGHT, i + 10, GROUND_HEIGHT + 5);
    }
    
    // 绘制地平线
    setlinecolor(nightMode ? RGB(40, 40, 50) : RGB(180, 200, 220));
    line(0, GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT);
}

// 生成障碍物
void generateObstacle() {
    // 条件1：检查障碍物数量上限
    if (obstacleCount >= MAX_OBSTACLES_ON_SCREEN) {
        return;
    }
    
    // 条件2：检查是否达到最小生成间隔
    if (framesSinceLastObstacle < nextSpawnInterval) {
        return;
    }
    
    // 条件3：查找可用的障碍物槽位
    int availableSlot = -1;
    for (int i = 0; i < 5; i++) {
        if (obstacles[i].passed) {
            availableSlot = i;
            break;
        }
    }
    
    if (availableSlot == -1) {
        return; // 没有可用槽位
    }
    
    // 条件4：检查与最近障碍物的距离
    int minDistance = SCREEN_WIDTH; // 初始化为屏幕宽度
    for (int i = 0; i < 5; i++) {
        if (!obstacles[i].passed) {
            // 计算与屏幕右侧的距离
            int distance = SCREEN_WIDTH - obstacles[i].x;
            if (distance < minDistance) {
                minDistance = distance;
            }
        }
    }
    
    // 如果已有障碍物且距离太近，则不生成新障碍物
    if (obstacleCount > 0 && minDistance < nextMinDistance) {
        // 强制生成机制：如果太久没生成，强制生成
        if (framesSinceLastObstacle > MAX_SPAWN_INTERVAL * 2) {
            // 强制生成，继续执行下面的生成逻辑
        } else {
            return; // 距离太近，不生成
        }
    }
    
    // 根据当前情况调整障碍物类型权重
    int type;
    int typeRand = rand() % 100;
    
    // 权重分配：小仙人掌40%，大仙人掌35%，鸟25%
    if (typeRand < 40) {
        type = 0; // 小仙人掌
    } else if (typeRand < 75) {
        type = 1; // 大仙人掌
    } else {
        type = 2; // 鸟
    }
    
    // 避免连续生成相同类型
    for (int i = 0; i < 5; i++) {
        if (!obstacles[i].passed && obstacles[i].type == type) {
            // 如果最近有相同类型，适当调整
            if (rand() % 2 == 0) { // 50%概率换类型
                type = (type + 1) % 3;
            }
            break;
        }
    }
    
    // 根据类型设置障碍物属性
    obstacles[availableSlot].type = type;
    obstacles[availableSlot].x = SCREEN_WIDTH;
    obstacles[availableSlot].passed = 0;
    
    if (type == 0) { // 小仙人掌
        obstacles[availableSlot].width = CACTUS_WIDTH;
        obstacles[availableSlot].height = CACTUS_HEIGHT;
        obstacles[availableSlot].y = GROUND_HEIGHT - CACTUS_HEIGHT;
    } else if (type == 1) { // 大仙人掌
        obstacles[availableSlot].width = CACTUS_WIDTH + 10;
        obstacles[availableSlot].height = CACTUS_HEIGHT + 20;
        obstacles[availableSlot].y = GROUND_HEIGHT - (CACTUS_HEIGHT + 20);
    } else { // 鸟
        obstacles[availableSlot].width = BIRD_WIDTH;
        obstacles[availableSlot].height = BIRD_HEIGHT;
        obstacles[availableSlot].y = GROUND_HEIGHT - 100 - (rand() % 50);
    }
    
    obstacleCount++;
    framesSinceLastObstacle = 0; // 重置生成计时器
    
    // 关键改进：每次生成障碍物后，随机设置下一次的生成参数
    // 根据游戏速度调整基础值
    int baseSpawnInterval = 35 - (gameSpeed - GAME_SPEED) * 2;
    if (baseSpawnInterval < MIN_SPAWN_INTERVAL) baseSpawnInterval = MIN_SPAWN_INTERVAL;
    
    int baseMinDistance = 350 - (gameSpeed - GAME_SPEED) * 10;
    if (baseMinDistance < MIN_OBSTACLE_DISTANCE) baseMinDistance = MIN_OBSTACLE_DISTANCE;
    
    // 添加随机变化
    nextSpawnInterval = baseSpawnInterval + rand() % 20; // 在基础值上±10帧
    nextMinDistance = baseMinDistance + rand() % 200;    // 在基础值上±100像素
    
    // 确保在合理范围内
    if (nextSpawnInterval < MIN_SPAWN_INTERVAL) nextSpawnInterval = MIN_SPAWN_INTERVAL;
    if (nextSpawnInterval > MAX_SPAWN_INTERVAL) nextSpawnInterval = MAX_SPAWN_INTERVAL;
    
    if (nextMinDistance < MIN_OBSTACLE_DISTANCE) nextMinDistance = MIN_OBSTACLE_DISTANCE;
    if (nextMinDistance > MAX_OBSTACLE_DISTANCE) nextMinDistance = MAX_OBSTACLE_DISTANCE;
}

// 更新障碍物
void updateObstacles() {
    // 更新现有障碍物位置并检查是否移出屏幕
    for (int i = 0; i < 5; i++) {
        if (!obstacles[i].passed) {
            obstacles[i].x -= gameSpeed;
            
            // 如果障碍物离开屏幕
            if (obstacles[i].x + obstacles[i].width < 0) {
                obstacles[i].passed = 1;
                obstacleCount--;
                // 关键修复：当障碍物移出屏幕时，立即重置生成计时器
                // 这样确保了在移出障碍物后可以快速生成新的障碍物
                framesSinceLastObstacle = nextSpawnInterval;
            }
            
            // 如果恐龙越过障碍物
            if (obstacles[i].x + obstacles[i].width < dino.x && !obstacles[i].passed) {
                obstacles[i].passed = 1;
                score += 1;
                
                // 每100分增加速度
                if (score % 100 == 0 && gameSpeed < 20) {
                    gameSpeed += 1;
                }
            }
        }
    }
    
    // 每帧增加生成计时器
    framesSinceLastObstacle++;
    
    // 简化生成逻辑：只要满足条件就尝试生成
    // 条件1：如果太久没生成，强制生成（无论当前障碍物数量）
    if (framesSinceLastObstacle > MAX_SPAWN_INTERVAL) {
        generateObstacle();
        return;
    }
    
    // 条件2：如果屏幕上障碍物很少，增加生成机会
    if (obstacleCount < MAX_OBSTACLES_ON_SCREEN && framesSinceLastObstacle > nextSpawnInterval) {
        // 基础概率随游戏速度增加
        int baseChance = 10 + (gameSpeed - GAME_SPEED) * 3; // 10%到40%
        
        // 添加随机变化：在基础概率上±5%
        int randomChance = baseChance - 5 + rand() % 11;
        if (randomChance < 5) randomChance = 5;
        
        if (rand() % 100 < randomChance) {
            generateObstacle();
            return;
        }
    }
    
    // 条件3：如果屏幕上没有障碍物，强制生成
    if (obstacleCount == 0 && framesSinceLastObstacle > 20) {
        generateObstacle();
        return;
    }
}

// 绘制障碍物
void drawObstacles() {
    for (int i = 0; i < 5; i++) {
        if (!obstacles[i].passed) {
            if (obstacles[i].type == 0 || obstacles[i].type == 1) { // 仙人掌
                // 绘制仙人掌
                setfillcolor(nightMode ? RGB(80, 120, 80) : RGB(100, 160, 100));
                fillrectangle(obstacles[i].x, obstacles[i].y, 
                             obstacles[i].x + obstacles[i].width, 
                             obstacles[i].y + obstacles[i].height);
                
                // 绘制仙人掌刺
                setlinecolor(nightMode ? RGB(60, 100, 60) : RGB(80, 140, 80));
                line(obstacles[i].x + obstacles[i].width / 2, obstacles[i].y + 5,
                     obstacles[i].x + obstacles[i].width / 2, obstacles[i].y + obstacles[i].height - 5);
                
                // 绘制仙人掌分支
                if (obstacles[i].type == 1) {
                    fillrectangle(obstacles[i].x - 5, obstacles[i].y + 20,
                                 obstacles[i].x, obstacles[i].y + 30);
                    fillrectangle(obstacles[i].x + obstacles[i].width, obstacles[i].y + 10,
                                 obstacles[i].x + obstacles[i].width + 5, obstacles[i].y + 20);
                }
            } else { // 鸟
                // 绘制鸟身体
                setfillcolor(nightMode ? RGB(120, 100, 120) : RGB(200, 100, 100));
                fillellipse(obstacles[i].x, obstacles[i].y,
                           obstacles[i].x + obstacles[i].width, obstacles[i].y + obstacles[i].height);
                
                // 绘制鸟翅膀（动画效果）
                setfillcolor(nightMode ? RGB(100, 80, 100) : RGB(180, 80, 80));
                if (frameCount % 20 < 10) {
                    fillrectangle(obstacles[i].x - 5, obstacles[i].y + 5,
                                 obstacles[i].x + 5, obstacles[i].y + obstacles[i].height - 5);
                } else {
                    fillrectangle(obstacles[i].x + obstacles[i].width - 5, obstacles[i].y + 5,
                                 obstacles[i].x + obstacles[i].width + 5, obstacles[i].y + obstacles[i].height - 5);
                }
                
                // 绘制鸟眼睛
                setfillcolor(RGB(255, 255, 255));
                solidcircle(obstacles[i].x + obstacles[i].width - 8, obstacles[i].y + 5, 3);
                setfillcolor(RGB(0, 0, 0));
                solidcircle(obstacles[i].x + obstacles[i].width - 7, obstacles[i].y + 5, 1);
            }
        }
    }
}

// 绘制云朵
void drawClouds() {
    for (int i = 0; i < cloudCount; i++) {
        setfillcolor(nightMode ? RGB(100, 100, 120) : RGB(250, 250, 255));
        // 绘制云朵（多个圆形组合）
        fillellipse(clouds[i].x, clouds[i].y, clouds[i].x + 40, clouds[i].y + 20);
        fillellipse(clouds[i].x + 15, clouds[i].y - 10, clouds[i].x + 55, clouds[i].y + 15);
        fillellipse(clouds[i].x + 30, clouds[i].y, clouds[i].x + 70, clouds[i].y + 20);
    }
}

// 更新云朵位置
void updateClouds() {
    for (int i = 0; i < cloudCount; i++) {
        clouds[i].x -= clouds[i].speed;
        
        // 如果云朵离开屏幕，重置到右侧
        if (clouds[i].x + 70 < 0) {
            clouds[i].x = SCREEN_WIDTH;
            clouds[i].y = 50 + rand() % 100;
            clouds[i].speed = 1 + rand() % 3;
        }
    }
}

// 检查碰撞
void checkCollision() {
    for (int i = 0; i < 5; i++) {
        if (!obstacles[i].passed) {
            // 简单矩形碰撞检测
            if (dino.x < obstacles[i].x + obstacles[i].width &&
                dino.x + dino.width > obstacles[i].x &&
                dino.y < obstacles[i].y + obstacles[i].height &&
                dino.y + dino.height > obstacles[i].y) {
                gameState = GAME_OVER;
            }
        }
    }
}

// 绘制分数
void drawScore() {
    char scoreText[50];
    sprintf(scoreText, "分数: %d", score);
    
    settextcolor(nightMode ? RGB(200, 200, 255) : RGB(80, 80, 120));
    settextstyle(20, 0, _T("Consolas"));
    outtextxy(SCREEN_WIDTH - 150, 20, scoreText);
    
    sprintf(scoreText, "最高分: %d", highScore);
    outtextxy(SCREEN_WIDTH - 150, 50, scoreText);
    
    // 显示游戏速度
    sprintf(scoreText, "速度: %d", gameSpeed);
    outtextxy(SCREEN_WIDTH - 150, 80, scoreText);
    
    // 显示模式
    if (nightMode) {
        settextcolor(RGB(150, 150, 255));
        outtextxy(SCREEN_WIDTH - 150, 110, _T("夜晚模式"));
    }
}

// 主函数
int main() {
    // 初始化图形
    initGraphics();
    
    // 初始化游戏
    initGame();
    
    // 游戏主循环
    while (true) {
        // 处理输入
        handleInput();
        
        // 更新游戏逻辑
        updateGame();
        
        // 渲染游戏
        renderGame();
        
        // 控制帧率
        static DWORD lastTime = GetTickCount();
        DWORD currentTime = GetTickCount();
        DWORD deltaTime = currentTime - lastTime;
        
        if (deltaTime < 1000 / TARGET_FPS) {
            Sleep(1000 / TARGET_FPS - deltaTime);
        }
        
        lastTime = GetTickCount();
    }
    
    // 关闭图形窗口
    EndBatchDraw();
    closegraph();
    
    return 0;
}
