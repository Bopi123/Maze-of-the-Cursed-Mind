#include <raylib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <stdio.h> /* for snprintf */

/*
 * Maze of the Cursed Mind
 *
 * - Start menu uses bg.png
 * - Character-select uses bgchar.png
 * - Maze screen uses mazebg.png as the full-screen background with the maze drawn on top
 * - HUD: full-width semi-transparent bar + shadowed text for readability (Option A)
 *
 * Place ABI.png, JOEBI.png, ICY.png, bg.png, bgchar.png, mazebg.png in the project folder.
 */

typedef enum { STATE_MENU, STATE_SELECT, STATE_GAME } GameState;
static GameState gameState = STATE_MENU;

#define MAZE_COLS 59
#define MAZE_ROWS 35
#define CELL_SIZE 21
#define MAX_TRAPS 30
#define ANSWER_COUNT 4
#define START_LIVES 3

static const Color MAZE_COL = (Color){111, 70, 133, 255};
static const Color WALL_COL = (Color){45, 28, 60, 255};

typedef struct { int row, col; } Player;
typedef struct {
    int r, c;
    bool active;
    const char *question;
    const char *choices[ANSWER_COUNT];
    int correctIndex;
} Trap;

static Texture2D texABI;
static Texture2D texJOEBI;
static Texture2D texICY;
static Texture2D texBG;      /* start menu background (bg.png) */
static Texture2D texBGCHAR;  /* character-select background (bgchar.png) */
static Texture2D texMAZEBG;  /* maze full-screen background (mazebg.png) */

static bool maze[MAZE_ROWS][MAZE_COLS];
static Trap traps[MAX_TRAPS];
static int playerShape = 1;

/* Utility */
static inline bool InBounds(int r,int c){ return r>=0 && r<MAZE_ROWS && c>=0 && c<MAZE_COLS; }

static Vector2 CellCenter(int r,int c){
    float ox=(GetScreenWidth()-MAZE_COLS*CELL_SIZE)*0.5f;
    float oy=(GetScreenHeight()-MAZE_ROWS*CELL_SIZE)*0.5f;
    return (Vector2){ ox + c*CELL_SIZE + CELL_SIZE*0.5f,
                      oy + r*CELL_SIZE + CELL_SIZE*0.5f };
}

/* Maze generation (unchanged) */
static void GenerateMaze(void){
    for(int r=0;r<MAZE_ROWS;r++)
        for(int c=0;c<MAZE_COLS;c++) maze[r][c]=false;

    typedef struct { int r,c; } Node;
    Node stack[MAZE_ROWS*MAZE_COLS];
    int top=0;

    maze[1][1]=true;
    stack[top++] = (Node){1,1};

    int dr[4]={-2,0,2,0};
    int dc[4]={0,2,0,-2};

    while(top>0){
        Node cur=stack[top-1];
        int dirs[4]={0,1,2,3};
        for(int i=3;i>0;i--){
            int j=rand()%(i+1);
            int t=dirs[i]; dirs[i]=dirs[j]; dirs[j]=t;
        }

        bool carved=false;
        for(int i=0;i<4;i++){
            int d=dirs[i];
            int nr=cur.r+dr[d];
            int nc=cur.c+dc[d];
            if(InBounds(nr,nc) && !maze[nr][nc]){
                maze[cur.r+dr[d]/2][cur.c+dc[d]/2]=true;
                maze[nr][nc]=true;
                stack[top++] = (Node){nr,nc};
                carved=true;
                break;
            }
        }
        if(!carved) top--;
    }

    for(int r=0;r<MAZE_ROWS;r++){
        maze[r][0]=false;
        maze[r][MAZE_COLS-1]=false;
    }
    for(int c=0;c<MAZE_COLS;c++){
        maze[0][c]=false;
        maze[MAZE_ROWS-1][c]=false;
    }
}

static void NearestPassage(int tr,int tc,int *outR,int *outC){
    int best=INT_MAX;
    int br=1, bc=1;

    for(int r=0;r<MAZE_ROWS;r++)
        for(int c=0;c<MAZE_COLS;c++)
            if(maze[r][c]){
                int d=abs(r-tr) + abs(c-tc);
                if(d<best){ best=d; br=r; bc=c; }
            }
    *outR=br;
    *outC=bc;
}

/* Question bank */
static const struct { const char*q; const char*opts[4]; int correct; } BANK[ ]={
    {"What does CPU stand for?",{"Central Processing Unit","Computer Power Unit","Central Program Utility","Control Processing Unit"},0},
    {"Which device stores data permanently?",{"RAM","SSD/HDD","Cache","GPU"},1},
    {"What does RAM stand for?",{"Random Access Memory","Read Access Module","Rapid Application Memory","Remote Access Machine"},0},
    {"Which is an OS?",{"YouTube","Windows","Chrome Extension","Visual Studio"},1},
    {"What does GPU stand for?",{"Graphics Processing Unit","General Performance Unit","Graphical Programming Utility","Gaming Performance Unit"},0},
    {"I can Fly, but no wings. I can cry but, I don't have eyes. What am I?",{"River","Steam","Cloud","Wind"},2},
    {"What has to be broken before you can use it?",{"Egg","Heart","Coconut","KitKat"},0},
    {"What kind of band that never plays music?",{"Silence","Rubber band","Signs","Trust"},1},
    {"What can be broken, even if you never pick it up or touch it?",{"Promise","Love","Heart","Glass"},0},
    {"The more of this there is, The less you see?",{"Mask","Pain","Darkness","Jealousy"},3}
};
static const int BANK_COUNT = sizeof(BANK)/sizeof(BANK[0]);

static int TrapAt(int r,int c){
    for(int i=0;i<MAX_TRAPS;i++)
        if(traps[i].active && traps[i].r==r && traps[i].c==c)
            return i;
    return -1;
}

static void PlaceTraps(int sr,int sc,int gr,int gc){
    int ps[MAZE_ROWS*MAZE_COLS][2];
    int n=0;

    for(int r=0;r<MAZE_ROWS;r++)
        for(int c=0;c<MAZE_COLS;c++)
            if(maze[r][c] && !(r==sr&&c==sc) && !(r==gr&&c==gc)){
                ps[n][0]=r;
                ps[n][1]=c;
                n++;
            }

    for(int i=n-1;i>0;i--){
        int j=rand()%(i+1);
        int tr=ps[i][0], tc=ps[i][1];
        ps[i][0]=ps[j][0]; ps[i][1]=ps[j][1];
        ps[j][0]=tr; ps[j][1]=tc;
    }

    for(int i=0;i<MAX_TRAPS;i++){
        traps[i].active=false;
    }

    int actual = n<MAX_TRAPS ? n : MAX_TRAPS;

    for(int i=0;i<actual;i++){
        int id=rand()%BANK_COUNT;
        traps[i]=(Trap){
            ps[i][0], ps[i][1], true,
            BANK[id].q,
            {BANK[id].opts[0],BANK[id].opts[1],BANK[id].opts[2],BANK[id].opts[3]},
            BANK[id].correct
        };
    }
}

static void FlipMaze(Player *p,int *goalR,int *goalC){
    int pr=p->row, pc=p->col;

    GenerateMaze();

    NearestPassage(pr,pc, &p->row,&p->col);
    NearestPassage(MAZE_ROWS-2,MAZE_COLS-2, goalR,goalC);

    PlaceTraps(p->row,p->col,*goalR,*goalC);
}

int main(void){
    InitWindow(GetMonitorWidth(0),GetMonitorHeight(0),"Maze of the Cursed Mind");
    ToggleFullscreen();
    SetTargetFPS(60);
    srand((unsigned)time(NULL));

    /* Load textures (make sure files are placed in the project folder) */
    texABI   = LoadTexture("ABI.png");
    texJOEBI = LoadTexture("JOEBI.png");
    texICY   = LoadTexture("ICY.png");
    texBG    = LoadTexture("bg.png");
    texBGCHAR= LoadTexture("bgchar.png");
    texMAZEBG= LoadTexture("mazebg.png");

    Player player;
    int goalR,goalC,initialDist;

    bool gameOver=false, won=false, inQuestion=false;
    int lives=START_LIVES, currentTrap=-1, selected=-1;

    bool f80=false,f60=false,f40=false,f20=false,f10=false;
    int shakeFrames=0;

    GenerateMaze();
    NearestPassage(1,1,&player.row,&player.col);
    NearestPassage(MAZE_ROWS-2,MAZE_COLS-2,&goalR,&goalC);
    initialDist = abs(goalR-player.row)+abs(goalC-player.col);
    PlaceTraps(player.row,player.col,goalR,goalC);

    while(!WindowShouldClose()){

        /* ---------------------- MENU ---------------------- */
        if(gameState==STATE_MENU){
            if(IsKeyPressed(KEY_ENTER))
                gameState=STATE_SELECT;

            const char *title="MAZE OF THE CURSED MIND";
            const char *t2="Press ENTER to choose character";
            const char *t3="Move: Arrows | Answer: 1-4 | R: Restart";

            BeginDrawing();

            /* Draw start menu background (bg.png) */
            if(texBG.id != 0){
                Rectangle src = { 0.0f, 0.0f, (float)texBG.width, (float)texBG.height };
                Rectangle dst = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
                DrawTexturePro(texBG, src, dst, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                ClearBackground((Color){20,15,40,255});
            }

            /* overlay for readability */
            DrawRectangle(0,0,GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.25f));

            int titleFontSize = 56;
            int smallFontSize = 22;
            int titleW = MeasureText(title, titleFontSize);
            float cx = GetScreenWidth()/2.0f;
            float titleY = 160.0f;

            DrawText(title, (int)(cx - titleW*0.5f), (int)titleY, titleFontSize, WHITE);

            int w2 = MeasureText(t2, smallFontSize);
            int w3 = MeasureText(t3, smallFontSize);
            DrawText(t2, (int)(cx - w2*0.5f), (int)(titleY + titleFontSize + 8), smallFontSize, LIGHTGRAY);
            DrawText(t3, (int)(cx - w3*0.5f), (int)(titleY + titleFontSize + 36), smallFontSize, GRAY);

            EndDrawing();
            continue;
        }

        /* ----------------- SELECT ----------------- */
        if(gameState==STATE_SELECT){
            BeginDrawing();

            /* Draw character-select background (bgchar.png) */
            if(texBGCHAR.id != 0){
                Rectangle src = { 0.0f, 0.0f, (float)texBGCHAR.width, (float)texBGCHAR.height };
                Rectangle dst = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
                DrawTexturePro(texBGCHAR, src, dst, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                ClearBackground((Color){20,15,40,255});
            }

            /* subtle overlay so text is readable */
            DrawRectangle(0,0,GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.18f));

            const char* t="SELECT YOUR CHARACTER";
            int tw = MeasureText(t,32);
            DrawText(t,GetScreenWidth()/2 - tw/2,120,32,WHITE);

            float cx=GetScreenWidth()/2.0f;
            float y=260;
            float spacing=240;
            float x1=cx-spacing;
            float x2=cx;
            float x3=cx+spacing;

            float ts=300;

            Rectangle srcABI  ={0,0,texABI.width,texABI.height};
            Rectangle srcJOEBI={0,0,texJOEBI.width,texJOEBI.height};
            Rectangle srcICY  ={0,0,texICY.width,texICY.height};

            Rectangle dstABI  ={ x1-ts/2, y-ts/4, ts,ts };
            Rectangle dstJOEBI={ x2-ts/2, y-ts/4, ts,ts };
            Rectangle dstICY  ={ x3-ts/2, y-ts/4, ts,ts };

            if(texABI.id!=0)   DrawTexturePro(texABI,srcABI,dstABI,(Vector2){0,0},0,WHITE);
            if(texJOEBI.id!=0) DrawTexturePro(texJOEBI,srcJOEBI,dstJOEBI,(Vector2){0,0},0,WHITE);
            if(texICY.id!=0)   DrawTexturePro(texICY,srcICY,dstICY,(Vector2){0,0},0,WHITE);

            int txtW;

            txtW=MeasureText("1 - ABI The Emoticon",25);
            DrawText("1 - ABI The Emoticon", x1-txtW/2, y+ts/2+29,20,WHITE);

            txtW=MeasureText("2 - JUBI The Gengsta",25);
            DrawText("2 - JUBI The Gengsta", x2-txtW/2, y+ts/2+29,20,WHITE);

            txtW=MeasureText("3 - CARLO the Sad Boi",25);
            DrawText("3 - CARLO the Sad Boi", x3-txtW/2, y+ts/2+29,20,WHITE);

            DrawText("  Will Abi find her peace\n outside of the maze?\n",
                     x1-MeasureText("  Will Abi find her\n peace outside of the maze?\n",16)/2,
                     y+ts/2+60, 16, LIGHTGRAY);

            DrawText("   Will Jubi be back in\n her territory in time?\n",
                     x2-MeasureText("   Will Jubi be back in\n her territory in time?\n",16)/2,
                     y+ts/2+60, 16, LIGHTGRAY);

            DrawText("     Will carlo discover his\n happines outside this maze?\n",
                     x3-MeasureText("     Will carlo discover his\n happines outside this maze?\n",16)/2,
                     y+ts/2+60, 16, LIGHTGRAY);

            if(IsKeyPressed(KEY_ONE))  { playerShape=1; gameState=STATE_GAME; }
            if(IsKeyPressed(KEY_TWO))  { playerShape=2; gameState=STATE_GAME; }
            if(IsKeyPressed(KEY_THREE)){ playerShape=3; gameState=STATE_GAME; }

            EndDrawing();
            continue;
        }

        /* ---------------------- GAME ---------------------- */

        if(IsKeyPressed(KEY_R)){
            GenerateMaze();
            NearestPassage(1,1,&player.row,&player.col);
            NearestPassage(MAZE_ROWS-2,MAZE_COLS-2,&goalR,&goalC);

            initialDist = abs(goalR-player.row)+abs(goalC-player.col);
            PlaceTraps(player.row,player.col,goalR,goalC);

            gameOver=false; won=false;
            lives=START_LIVES;
            selected=-1; currentTrap=-1;
            inQuestion=false;
            f80=f60=f40=f20=f10=false;
        }

        if(!gameOver && !won){

            if(!inQuestion){
                int dr=0,dc=0;
                if(IsKeyPressed(KEY_RIGHT)) dc=1;
                if(IsKeyPressed(KEY_LEFT))  dc=-1;
                if(IsKeyPressed(KEY_DOWN))  dr=1;
                if(IsKeyPressed(KEY_UP))    dr=-1;

                int nr=player.row+dr;
                int nc=player.col+dc;

                if((dr||dc) && InBounds(nr,nc) && maze[nr][nc]){
                    player.row=nr;
                    player.col=nc;

                    int t=TrapAt(nr,nc);
                    if(t>=0){
                        inQuestion=true;
                        currentTrap=t;
                        selected=-1;
                    }

                    int d=abs(goalR-player.row)+abs(goalC-player.col);

                    int t80 = initialDist*4/5;
                    int t60 = initialDist*3/5;
                    int t40 = initialDist*2/5;
                    int t20 = initialDist*1/5;
                    int t10 = initialDist/10;

                    if(!f80 && d<=t80){ FlipMaze(&player,&goalR,&goalC); f80=true; initialDist=abs(goalR-player.row)+abs(goalC-player.col); }
                    else if(!f60 && d<=t60){ FlipMaze(&player,&goalR,&goalC); f60=true; initialDist=abs(goalR-player.row)+abs(goalC-player.col); }
                    else if(!f40 && d<=t40){ FlipMaze(&player,&goalR,&goalC); f40=true; initialDist=abs(goalR-player.row)+abs(goalC-player.col); }
                    else if(!f20 && d<=t20){ FlipMaze(&player,&goalR,&goalC); f20=true; initialDist=abs(goalR-player.row)+abs(goalC-player.col); }
                    else if(!f10 && d<=t10){ FlipMaze(&player,&goalR,&goalC); f10=true; initialDist=abs(goalR-player.row)+abs(goalC-player.col); }
                }
            } else {
                if(IsKeyPressed(KEY_ONE))   selected=0;
                if(IsKeyPressed(KEY_TWO))   selected=1;
                if(IsKeyPressed(KEY_THREE)) selected=2;
                if(IsKeyPressed(KEY_FOUR))  selected=3;

                if(selected!=-1){
                    if(selected==traps[currentTrap].correctIndex){
                        traps[currentTrap].active=false;
                    } else {
                        lives--;
                        traps[currentTrap].active=false;
                        shakeFrames=10;
                        if(lives<=0) gameOver=true;
                    }
                    inQuestion=false;
                    currentTrap=-1;
                    selected=-1;
                }
            }

            if(player.row==goalR && player.col==goalC)
                won=true;
        }

        /* --------------------------- DRAW GAME --------------------------- */
        BeginDrawing();

        /* Draw the maze background (full screen) */
        if(texMAZEBG.id != 0){
            Rectangle src = { 0.0f, 0.0f, (float)texMAZEBG.width, (float)texMAZEBG.height };
            Rectangle dst = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
            DrawTexturePro(texMAZEBG, src, dst, (Vector2){0,0}, 0.0f, WHITE);
        } else {
            ClearBackground(RAYWHITE); /* fallback */
        }

        int sx=0, sy=0;
        if(shakeFrames>0){
            sx=GetRandomValue(-5,5);
            sy=GetRandomValue(-5,5);
            shakeFrames--;
        }

        float ox=(GetScreenWidth()-MAZE_COLS*CELL_SIZE)*0.5f + sx;
        float oy=(GetScreenHeight()-MAZE_ROWS*CELL_SIZE)*0.5f + sy;

        /* Draw maze tiles on top of the background */
        for(int r=0;r<MAZE_ROWS;r++){
            for(int c=0;c<MAZE_COLS;c++){
                Rectangle rc={ ox+c*CELL_SIZE, oy+r*CELL_SIZE, CELL_SIZE, CELL_SIZE };
                DrawRectangleRec(rc, maze[r][c] ? MAZE_COL : WALL_COL);
            }
        }

        Vector2 g=CellCenter(goalR,goalC);
        g.x+=sx;
        g.y+=sy;

        DrawCircleV(g, CELL_SIZE*0.4f, GOLD);
        DrawText("GOAL", g.x-MeasureText("GOAL",12)/2, g.y-6,12,BLACK);

        Vector2 p=CellCenter(player.row,player.col);
        p.x+=sx;
        p.y+=sy;

        float charSize = CELL_SIZE*1.2f;
        Rectangle dst = { p.x-charSize/2, p.y-charSize/2, charSize,charSize };

        if(playerShape==1 && texABI.id!=0)
            DrawTexturePro(texABI,(Rectangle){0,0,texABI.width,texABI.height},dst,(Vector2){0,0},0,WHITE);
        else if(playerShape==2 && texJOEBI.id!=0)
            DrawTexturePro(texJOEBI,(Rectangle){0,0,texJOEBI.width,texJOEBI.height},dst,(Vector2){0,0},0,WHITE);
        else if(playerShape==3 && texICY.id!=0)
            DrawTexturePro(texICY,(Rectangle){0,0,texICY.width,texICY.height},dst,(Vector2){0,0},0,WHITE);

        int active=0;
        for(int i=0;i<MAX_TRAPS;i++)
            if(traps[i].active) active++;

        /* ---------- Readable Full-width HUD (Option A) ---------- */
        {
            float hudPadding = 10.0f;
            float hudHeight  = 36.0f;
            Rectangle hudRect = { hudPadding + sx, 6.0f + sy, GetScreenWidth() - hudPadding*2.0f, hudHeight };

            /* dark semi-transparent bar (full width) */
            DrawRectangleRounded(hudRect, 0.12f, 6, Fade(BLACK, 0.65f));

            /* draw text with 1px shadow */
            #define DRAW_TEXT_SHADOW(txt, x, y, size, col) \
                DrawText(txt, (x)+1, (y)+1, (size), Fade(BLACK, 0.55f)); \
                DrawText(txt, (x),     (y),     (size), (col));

            const char *leftStr = "Move: Arrows | Answer: 1-4 | R: Restart";
            char livesStr[32]; snprintf(livesStr, sizeof(livesStr), "Lives: %d", lives);
            char trapsStr[32]; snprintf(trapsStr, sizeof(trapsStr), "Traps: %d", active);

            /* Use Color variables to avoid macro comma-splitting issue */
            Color leftCol  = LIGHTGRAY;
            Color livesCol = LIGHTGRAY;
            Color trapsCol = (Color){240,240,240,255};

            float leftX  = hudRect.x + 12.0f;
            float midY   = hudRect.y + 6.0f; /* vertical position for text */
            float rightX = hudRect.x + hudRect.width - 12.0f;

            DRAW_TEXT_SHADOW(leftStr, (int)leftX, (int)midY, 14, leftCol);

            int wLives = MeasureText(livesStr, 14);
            float centerX = hudRect.x + hudRect.width*0.5f - wLives*0.5f;
            DRAW_TEXT_SHADOW(livesStr, (int)centerX, (int)midY, 14, livesCol);

            int wTraps = MeasureText(trapsStr, 14);
            DRAW_TEXT_SHADOW(trapsStr, (int)(rightX - wTraps), (int)midY, 14, trapsCol);

            #undef DRAW_TEXT_SHADOW
        }

        /* ----------------------- YOU WIN SCREEN ----------------------- */
        if(won){
            DrawRectangle(GetScreenWidth()/2-300,GetScreenHeight()/2-80,600,160,Fade(GREEN,0.9f));

            int cx = GetScreenWidth()/2;
            int cy = GetScreenHeight()/2;

            int tw = MeasureText("YOU WIN!",42);

            DrawText("YOU WIN!",
                     cx - tw/2 + 60,
                     cy - 40,
                     42,
                     BLACK);

            int tw2 = MeasureText("Press R to play again",22);
            DrawText("Press R to play again",
                     cx - tw2/2 + 60,
                     cy + 20,
                     22,
                     BLACK);

            float iconSize = 140;
            Rectangle popDst = {
                cx - tw/2 - iconSize - 20,
                cy - iconSize/2,
                iconSize, iconSize
            };

            Rectangle src={0,0,0,0};

            if(playerShape==1 && texABI.id){
                src = (Rectangle){0,0,texABI.width,texABI.height};
                DrawTexturePro(texABI,src,popDst,(Vector2){0,0},0,WHITE);
            }
            else if(playerShape==2 && texJOEBI.id){
                src = (Rectangle){0,0,texJOEBI.width,texJOEBI.height};
                DrawTexturePro(texJOEBI,src,popDst,(Vector2){0,0},0,WHITE);
            }
            else if(playerShape==3 && texICY.id){
                src = (Rectangle){0,0,texICY.width,texICY.height};
                DrawTexturePro(texICY,src,popDst,(Vector2){0,0},0,WHITE);
            }
        }

        /* ----------------------- GAME OVER SCREEN ----------------------- */
        if(gameOver){
            DrawRectangle(GetScreenWidth()/2-300,GetScreenHeight()/2-80,600,160,Fade(BLACK,0.85f));

            int cx = GetScreenWidth()/2;
            int cy = GetScreenHeight()/2;

            int tw = MeasureText("GAME OVER",42);

            DrawText("GAME OVER",
                     cx - tw/2 + 60,
                     cy - 40,
                     42,
                     RED);

            int tw2 = MeasureText("Press R to restart",22);
            DrawText("Press R to restart",
                     cx - tw2/2 + 60,
                     cy + 20,
                     22,
                     WHITE);

            float iconSize = 140;
            Rectangle popDst = {
                cx - tw/2 - iconSize - 20,
                cy - iconSize/2,
                iconSize, iconSize
            };

            Rectangle src={0,0,0,0};

            if(playerShape==1 && texABI.id){
                src = (Rectangle){0,0,texABI.width,texABI.height};
                DrawTexturePro(texABI,src,popDst,(Vector2){0,0},0,WHITE);
            }
            else if(playerShape==2 && texJOEBI.id){
                src = (Rectangle){0,0,texJOEBI.width,texJOEBI.height};
                DrawTexturePro(texJOEBI,src,popDst,(Vector2){0,0},0,WHITE);
            }
            else if(playerShape==3 && texICY.id){
                src = (Rectangle){0,0,texICY.width,texICY.height};
                DrawTexturePro(texICY,src,popDst,(Vector2){0,0},0,WHITE);
            }
        }

        /* ----------------------- QUESTION UI ----------------------- */
        if(inQuestion && currentTrap>=0){
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.6f));
            DrawText("IT Question:",40,80,20,RAYWHITE);
            DrawText(traps[currentTrap].question,40,120,20,RAYWHITE);

            for(int i=0;i<ANSWER_COUNT;i++){
                DrawText(TextFormat("%d) %s",i+1,traps[currentTrap].choices[i]),
                         60,160+i*28,20,LIGHTGRAY);
            }
            DrawText("Press 1-4 to answer",40,160+ANSWER_COUNT*28+16,16,RAYWHITE);
        }

        EndDrawing();
    }

    if(texABI.id) UnloadTexture(texABI);
    if(texJOEBI.id) UnloadTexture(texJOEBI);
    if(texICY.id) UnloadTexture(texICY);
    if(texBG.id) UnloadTexture(texBG);
    if(texBGCHAR.id) UnloadTexture(texBGCHAR);
    if(texMAZEBG.id) UnloadTexture(texMAZEBG);

    CloseWindow();
    return 0;
}
