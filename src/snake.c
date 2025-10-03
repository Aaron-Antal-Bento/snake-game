#include "raylib.h" // raylib functions and types
#include <stdio.h>  // c standard library functions and types
#include <stdlib.h> // malloc
#include <time.h>   // rand/time

// Board contents
#define EMPTY 0
#define BOARDWALL 1
#define SNAKEBODY 2 // Segment of snake (Renders a sprite animation entering and leaving the cell)
#define FOOD 3

// Head and snake directions
#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3
#define NOTSET -1 // Used when user has not specified and input of current snake move cycle

// Game states
#define STARTMENU 0      // Goes to GAME state
#define GAME 1           // Goes to WIN, DEATHANIMATION or STARTMENU (if the game is reset)
#define DEATHSCREEN 2    // Goes to STARTMENU
#define WINSCREEN 3      // Goes to STARTMENU
#define DEATHANIMATION 4 // Set when snake has just died and is displaying a death animation before going to DEATHSCREEN

// Snake mouth state
#define CLOSED 0  // Default
#define OPENING 1 // Opens when in a cell adjecent to a cell containing food
#define CLOSING 2 // Closes on the snake move after opening if the snake hasnt eaten food
#define EATING 3  // Snake is eating food (Goes to CLOSED state as eating animation includes closing snakes mouth)

Color DARKERLIGHTGRAY = (Color){180, 180, 180, 255}; // One of the alternating background colours (the other is default raylib LIGHTGRAY)

// Global variables
int boardSize; // Width and height of the playable board + 2 for the boarder
const int screenWidth = 1024;
const int screenHeight = 576;
int gameState = STARTMENU;    // Stores the state of the game (STARTMENU, GAME, WIN, DEATH, DEATHANIMATION)
int scoreAchieved = 2;        // Stores the score the player achieved after the player has died
float pausedStartTime = 0.0f; // Stores the start time that the game was paused at
// Stores the length of time the game was paused for
float totalPausedTime = 0.0f; // Used to account for the time spent in the pause menu when calculating when the snake next needs to move
int DeathType = BOARDWALL;    // Stores the way the snake died (Hitting a wall or hitting the snake) used to display the correct death animation

typedef struct
{
    int x;
    int y;
} Position;

typedef struct
{
    Texture2D main;
    Texture2D left;
    Texture2D right;

    // For snake mouth eat and close animations as they are made of three layers (main, layer1, layer2)
    Texture2D layer1;
    Texture2D layer2;
} SpriteVariants; // Stores the different varients of a sprite such as turning left varient

typedef struct
{
    SpriteVariants enteringCell; // Holdes the entering sprite varients for the current collection
    SpriteVariants leavingCell;  // Holdes the leaving sprite varients for the current collection
} SpriteCollection;              // Holds a set of sprites that together make a full cell

int snakeMouthState = CLOSED; // Stores the state the snakes mouth is currently in
int snakeSpriteFrame = 0;     // Stores the current animation frame that all the parts of the snake are in
Texture2D FoodSprite;
Texture2D SnakeDeathWallSprite;  // When snake hits a wall
Texture2D SnakeDeathSnakeSprite; // When snake hits itself

// Snake front sprites
SpriteCollection SnakeHeadSprites;       // Mouth closed
SpriteCollection SnakeMouthEatSprites;   // Eating (same as closing but with food sprite)
SpriteCollection SnakeMouthCloseSprites; // Closing (same as eating but without food)
SpriteCollection SnakeMouthOpenSprites;  // Opening

SpriteCollection SnakeBodySprites;
SpriteCollection SnakeTailSprites;
SpriteCollection TailLengthenSprites; // For when snake is eating
SpriteCollection EmptyCellSprites;    // When the snakes front or tail is entering or leaving an empty cell
int tailPointDirection;               // Store the direction the tail was last pointed in to be used when the snake is lengthening

// Sounds
Sound ButtonClick;
Sound StartGame;
Sound WinGame;
Sound SwitchScreen;
Sound SnakeEat;
Sound SnakeDeath;

// Board cell
typedef struct
{
    int contents;                    // Contents of a cell in the board (EMPTY, WALL, SNAKE...)
    Position index;                  // Index in the 2d array 'board'
    Texture2D spriteEnteringCell;    // Holdes the sprite animation entering the cell
    Texture2D spriteLeavingCell;     // Holdes the sprite animation leaving the cell
    int snakeSpriteDirection;        // Stores the direction of the sprite in this cell
    int snakeSpriteDirectionLeaving; // Stores the direction of the sprite leaving the cell (used when snakes head and tail are entering and leaving the same cell)

    bool multipleLayers; // For snake mouth eat and close
    Texture2D layer1;    // Food layer (no rotation, only displayed while eating)
    Texture2D layer2;    // Mouth top layer
} Cell;

// Head of the snake is not displayed but is set based on the users input and guides the snake where to go when it makes its next move
typedef struct
{
    int snakeDir; // Direction the snake is currently going in based on the last snake movement
    // The direction of the head given by the players input (up, down, left, right and -1 for not set)
    int headDir; // Used to store the direction of the snake so it cannot go back on itself
    // Used to allow the player to store a move wheich will be moved into 'headDir' when the nake makes its next move
    int headDirBuffer;   // Makes user inputs feel more responsive and stops inputs being missed
    Position snakeFront; // The index of the front of the snake that the head rotates around
} SnakeHead;

typedef struct
{
    SnakeHead head;       // Head which is invisible and controls where the front of the snake will go when it moves
    int tailIndex;        // Keep track of the tail so it can be deleted when the snake moves. Extended when the snake eats to lengthen the snake
    Cell **snakeSegments; // List of pointers to board cells that make up all the parts of the snake (Head, front and body)
} Snake;

// Structure to hold relevent data and state of a button
typedef struct
{
    bool hovered;
    bool selected;
    Rectangle shape; // Size and position of the button
    char *text;      // Contents of the button
} Button;

Position findPos(Cell *itemRef)
{
    // returns the x and y indexes of a cell
    return itemRef->index;
}
Position getNextCellFromDir(Position pos, int Dir)
{
    // Returns the index of the adjacent cell to 'pos' in a specified direction (UP, DOWN, LEFT, RIGHT)
    if (Dir == LEFT)
        pos.x -= 1;
    else if (Dir == RIGHT)
        pos.x += 1;

    if (Dir == UP)
        pos.y -= 1;
    else if (Dir == DOWN)
        pos.y += 1;

    return pos;
}
float spriteRoatationFromDirection(int Dir)
{
    // Converts direction (LEFT, RIGHT, UP, DOWN) to a rotaion float
    // Returns an angle used to rotate the snake sprite by (based on a direction)
    if (Dir == UP)
        return 90.0f;
    else if (Dir == DOWN)
        return -90.0f;
    else if (Dir == LEFT)
        return 0.0f;
    else if (Dir == RIGHT)
        return 180.0f;

    return 0.0f;
}
int getTurnDirection(int currentDirection, int lastDirection)
{
    // Finds the turn direction of a snake segment (LEFT, RIGHT or NOTSET) based on the current cells direction and the direction of the next segement down in the snake
    int leftTurn = UP;    // Stores the direction required for a left turn (changed based on 'currentDirection')
    int rightTurn = DOWN; // Stores the direction required for a right turn (changed based on 'currentDirection')

    // Selects what turn direction is required to make either a right of left turn
    if (currentDirection == UP)
    {
        leftTurn = RIGHT;
        rightTurn = LEFT;
    }
    else if (currentDirection == DOWN)
    {
        leftTurn = LEFT;
        rightTurn = RIGHT;
    }
    else if (currentDirection == LEFT)
    {
        leftTurn = UP;
        rightTurn = DOWN;
    }
    else if (currentDirection == RIGHT)
    {
        leftTurn = DOWN;
        rightTurn = UP;
    }

    if (lastDirection == leftTurn) // If lastdirection matches the direction required for a left turn
        return LEFT;
    else if (lastDirection == rightTurn) // If lastdirection matches the direction required for a left turn
        return RIGHT;

    return NOTSET; // Returns not set if the snake is not turning
}
const char *DifficultyToString(int difficulty)
{
    // Converts the difficulty int to a string to display to the user
    if (difficulty == 0)
        return "EASY";
    else if (difficulty == 1)
        return "MEDIUM";
    else if (difficulty == 2)
        return "HARD";
    else if (difficulty == 3)
        return "EXPERT";

    return "";
}
int numCellsToFill(Snake *snake, Cell board[boardSize][boardSize])
{
    // Returns the number of EMPTY cells in the board (same as total cells - (snake length - snake head and tail(2)))
    int boardDimentions = boardSize - 2;                     // Dont include board walls
    int totalBoardCells = boardDimentions * boardDimentions; // Get how many cell need to be filled for a win (all cells)
    return totalBoardCells - (snake->tailIndex - 2);
}
bool isAdjacentToFood(Cell *cell, Cell board[boardSize][boardSize])
{
    // Returns true if the snakes front is in a cell that is directly (not diagonally) adjecent to a cell containing food
    if (board[cell->index.x + 1][cell->index.y].contents == FOOD)
        return true;
    if (board[cell->index.x - 1][cell->index.y].contents == FOOD)
        return true;
    if (board[cell->index.x][cell->index.y + 1].contents == FOOD)
        return true;
    if (board[cell->index.x][cell->index.y - 1].contents == FOOD)
        return true;
    return false;
}
bool isSameCell(Cell cell1, Cell cell2)
{
    // Returns true if the indexs match for cell1 and cell2 (to check if snakes head is entering the cell that the tail is leaving)
    Position index1 = findPos(&cell1);
    Position index2 = findPos(&cell2);

    if (index1.x == index2.x && index1.y == index2.y)
    {
        return true;
    }
    return false;
}

void SetEnteringAndLeavingSprite(SpriteCollection entering, SpriteCollection leaving, Snake *snake, int segmentIndx)
{
    int turnDirection = NOTSET;
    // Sets the entering and leaving texture for a cell and chooses which SpriteVarient to use based off the turn direction of the cell
    if (snake->snakeSegments[segmentIndx + 1] != NULL)
        turnDirection = getTurnDirection(snake->snakeSegments[segmentIndx]->snakeSpriteDirection, snake->snakeSegments[segmentIndx + 1]->snakeSpriteDirectionLeaving);

    if (turnDirection == NOTSET)
    {
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.main;
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.main;
    }
    else if (turnDirection == LEFT)
    {
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.left;
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.left;
    }
    else if (turnDirection == RIGHT)
    {
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.right;
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.right;
    }
}
void SetEnteringAndLeavingSpriteNoDir(SpriteCollection entering, SpriteCollection leaving, Cell *snakeSegment)
{
    // Sets the entering and leaving texture for a cell
    snakeSegment->spriteEnteringCell = entering.enteringCell.main;
    snakeSegment->spriteLeavingCell = leaving.leavingCell.main;
}
void SetEnteringSprite(SpriteCollection entering, Snake *snake, int segmentIndx)
{
    int turnDirection = NOTSET;
    // Sets the just the entering texture for a cell and chooses which SpriteVarient to use based off the turn direction of the cell
    if (snake->snakeSegments[segmentIndx + 1] != NULL)
        turnDirection = getTurnDirection(snake->snakeSegments[segmentIndx]->snakeSpriteDirection, snake->snakeSegments[segmentIndx + 1]->snakeSpriteDirectionLeaving);

    if (turnDirection == NOTSET)
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.main;
    else if (turnDirection == LEFT)
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.left;
    else if (turnDirection == RIGHT)
        snake->snakeSegments[segmentIndx]->spriteEnteringCell = entering.enteringCell.right;
}
void SetEnteringAndLeavingSpriteWithDir(SpriteCollection entering, SpriteCollection leaving, Cell *snakeSegment, int previousTurnDirection)
{
    // Used when using the stored tailPointDirection to keep the tails direction when the snake is lengthening
    // Sets the entering and leaving texture for a cell and chooses which SpriteVarient to use based off the turn direction of the cell
    int turnDirection = getTurnDirection(snakeSegment->snakeSpriteDirection, previousTurnDirection);
    if (turnDirection == NOTSET)
    {
        snakeSegment->spriteEnteringCell = entering.enteringCell.main;
        snakeSegment->spriteLeavingCell = leaving.leavingCell.main;
    }
    else if (turnDirection == LEFT)
    {
        snakeSegment->spriteEnteringCell = entering.enteringCell.left;
        snakeSegment->spriteLeavingCell = leaving.leavingCell.left;
    }
    else if (turnDirection == RIGHT)
    {
        snakeSegment->spriteEnteringCell = entering.enteringCell.right;
        snakeSegment->spriteLeavingCell = leaving.leavingCell.right;
    }
}
void SetLeavingSprite(SpriteCollection leaving, Snake *snake, int segmentIndx)
{
    int turnDirection = NOTSET;
    // Sets the just the leaving texture for a cell and chooses which SpriteVarient to use based off the turn direction of the cell
    if (snake->snakeSegments[segmentIndx + 1] != NULL)
        turnDirection = getTurnDirection(snake->snakeSegments[segmentIndx]->snakeSpriteDirection, snake->snakeSegments[segmentIndx + 1]->snakeSpriteDirection);
    if (turnDirection == NOTSET)
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.main;
    else if (turnDirection == LEFT)
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.left;
    else if (turnDirection == RIGHT)
        snake->snakeSegments[segmentIndx]->spriteLeavingCell = leaving.leavingCell.right;
}

void DrawCenteredText(const char *text, int y, int fontSize, Color color, int screenWidth)
{
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (screenWidth - textWidth) / 2, y, fontSize, color);
}
void DrawButton(Button button)
{
    int width = button.shape.width;
    int height = button.shape.height;
    int x = button.shape.x;
    int y = button.shape.y;

    char *text = button.text;
    int fontsize = 25;

    Color buttonColour = DARKGRAY;
    Color textColour = WHITE;
    Color selectedColour = {220, 220, 220, 255}; // very light gray

    if (button.hovered == true)
    {
        // Increase size slightly
        width += 6;
        height += 6;
        x -= 3;
        y -= 3;
        fontsize += 2;
    }

    DrawRectangle(x + 5, y, width - 10, height, buttonColour);
    DrawRectangle(x, y + 5, width, height - 10, buttonColour);
    if (button.selected == true)
    {
        // Add a white inner box if the button is selected
        DrawRectangle(x + 10, y + 5, width - 20, height - 10, selectedColour);
        DrawRectangle(x + 5, y + 10, width - 10, height - 20, selectedColour);
        textColour = DARKGRAY;
    }
    // Center text
    int textWidth = MeasureText(text, fontsize);
    int textHeight = fontsize;
    DrawText(text, x + (width / 2) - (textWidth / 2), y + (height / 2) - (textHeight / 2), fontsize, textColour);
}
void AnimateSprite(int X, int Y, int cellSize, Texture2D snakeSprite, int spriteDirection, int spriteFrame)
{
    Rectangle spriteSection = {snakeSprite.width / 5 * spriteFrame, 0, snakeSprite.width / 5, snakeSprite.height}; // Selects the correct section from the texture (each snake sprite has 5 frames)
    Rectangle drawLocation = {X + (float)cellSize / 2, Y + (float)cellSize / 2, cellSize, cellSize};               // Chooses the center of the cell to draw the sprite section at
    Vector2 origin = {(float)cellSize / 2, (float)cellSize / 2};                                                   // Sets origin to center of the cell to allow rotation
    float rotation = spriteRoatationFromDirection(spriteDirection);                                                // Finds rotaion based on the direction the snake was facing when it went throught that cell
    DrawTexturePro(snakeSprite, spriteSection, drawLocation, origin, rotation, WHITE);                             // Draw sprite
}
void AnimateLongSprite(int X, int Y, int cellSize, Texture2D snakeSprite, int spriteDirection, int spriteFrame)
{
    // Used to draw the 40x20 sprite used in snakes death animation when hitting a part of the snake
    Rectangle spriteSection = {snakeSprite.width / 5 * spriteFrame, 0, snakeSprite.width / 5, snakeSprite.height};
    Rectangle drawLocation = {X + (float)cellSize / 2, Y + (float)cellSize / 2, cellSize * 2, cellSize};
    Vector2 origin = {(float)cellSize * 1.5f, (float)cellSize / 2};
    float rotation = spriteRoatationFromDirection(spriteDirection);
    DrawTexturePro(snakeSprite, spriteSection, drawLocation, origin, rotation, WHITE);
}
bool IsMouseOverButton(Button button)
{
    // Check the position of the mouse
    Vector2 mousePos = GetMousePosition();
    // Return true if mouse if over specified button
    return CheckCollisionPointRec(mousePos, button.shape);
}
void CheckButtonHover(Button *button)
{
    if (IsMouseOverButton(*button))
        button->hovered = true;
    else
        button->hovered = false;
}

void generateFood(Cell board[boardSize][boardSize], Snake *snake)
{
    if (numCellsToFill(snake, board) != 0) // Make sure the player hasnt won and that there are spaces to place food
    {
        int x, y;
        do
        {
            // Keep generating x and y coordinates until a vaild cell is found
            x = rand() % boardSize;
            y = rand() % boardSize;
        } while (board[x][y].contents != EMPTY || &board[x][y] == snake->snakeSegments[0]); // Ensure food is placed on an empty cell
        board[x][y].contents = FOOD; // Set the found cell to contain food
    }
}
void cleanup(Snake *snake)
{
    // Clears the dynamic list in snake
    if (snake->snakeSegments != NULL)
    {
        free(snake->snakeSegments);
        snake->snakeSegments = NULL; // Avoid dangling pointer
    }
}

void initSnake(Snake *snake, Cell board[boardSize][boardSize])
{
    // Clear 'snakeSegments' list so it is ready to use again
    cleanup(snake);
    // Allocate the amount of memory needed to store the maximum length of the snake based on the size of the board
    snake->snakeSegments = (Cell **)malloc(boardSize * boardSize * sizeof(Cell *));

    // Store pointers to the board elements that make up the snake
    for (int i = 0; i < boardSize * boardSize; i++)
    {
        snake->snakeSegments[i] = NULL;
    }
    snake->snakeSegments[0] = &board[boardSize / 2 - 1][boardSize / 2]; // Head which is invisible and controls where the front of the snake will go when it moves
    snake->snakeSegments[1] = &board[boardSize / 2][boardSize / 2];     // Snake front: first displayed part of the snake
    snake->snakeSegments[1]->snakeSpriteDirection = LEFT;
    snake->snakeSegments[1]->contents = SNAKEBODY;
    snake->snakeSegments[2] = &board[boardSize / 2 + 1][boardSize / 2]; // Snake body (the snake will start with a length of 2)
    snake->snakeSegments[2]->snakeSpriteDirection = LEFT;
    snake->snakeSegments[2]->snakeSpriteDirectionLeaving = LEFT;
    snake->snakeSegments[2]->contents = SNAKEBODY;
    snake->snakeSegments[3] = &board[boardSize / 2 + 2][boardSize / 2]; // Snake body (the snake will start with a length of 2)
    snake->snakeSegments[3]->snakeSpriteDirection = LEFT;
    snake->snakeSegments[3]->snakeSpriteDirectionLeaving = LEFT;
    snake->snakeSegments[3]->contents = SNAKEBODY;

    Position snakeFrontPos = snake->snakeSegments[1]->index;     // Store the index of the cell that contains the front of the snake so the head can rotate around it when turning
    SnakeHead snakeHead = {LEFT, NOTSET, NOTSET, snakeFrontPos}; // Create the head of the snake with a direction of LEFT and no user inputs set
    snake->head = snakeHead;                                     // Assign the head to the snake

    snake->tailIndex = 4;     // Set the tail index (used to delete the tail of the snake when moving)
    snakeMouthState = CLOSED; // Set default mouth state
}
void createBoard(Cell board[boardSize][boardSize])
{
    // create empty board with walls all around it
    for (int i = 0; i < boardSize; i++)
    {
        for (int j = 0; j < boardSize; j++)
        {
            board[i][j].index = (Position){i, j};
            if (i == 0 || j == 0 || i == boardSize - 1 || j == boardSize - 1) // if current cell is at the edge of the board make it a wall
            {
                board[i][j].contents = BOARDWALL;
            }
            else
            {
                board[i][j].contents = EMPTY;
                board[i][j].snakeSpriteDirection = NOTSET; // Reset sprite direction
            }
        }
    }
}
void LoadSprites()
{
    // Leaving or filling an empty cell (snakes front or tail)
    Texture2D emptySprite = LoadTexture("resources/snakeSprites/EnterOrLeaveEmpty.png");
    EmptyCellSprites.enteringCell.main = emptySprite;
    EmptyCellSprites.enteringCell.left = emptySprite;
    EmptyCellSprites.enteringCell.right = emptySprite;
    EmptyCellSprites.leavingCell.main = emptySprite;
    EmptyCellSprites.leavingCell.left = emptySprite;
    EmptyCellSprites.leavingCell.right = emptySprite;

    // Snake Head
    SnakeHeadSprites.enteringCell.main = LoadTexture("resources/snakeSprites/HeadEnter.png");

    SnakeHeadSprites.leavingCell.main = LoadTexture("resources/snakeSprites/HeadLeave.png");
    SnakeHeadSprites.leavingCell.left = LoadTexture("resources/snakeSprites/HeadLeaveLeft.png");
    SnakeHeadSprites.leavingCell.right = LoadTexture("resources/snakeSprites/HeadLeaveRight.png");

    // Snake death
    SnakeDeathWallSprite = LoadTexture("resources/snakeSprites/HeadDeathWall.png");
    SnakeDeathSnakeSprite = LoadTexture("resources/snakeSprites/HeadDeathSnakebody.png");

    // Snake Eat
    SnakeMouthEatSprites.enteringCell.main = LoadTexture("resources/snakeSprites/HeadEatEnterBottom.png");
    SnakeMouthEatSprites.enteringCell.layer1 = LoadTexture("resources/snakeSprites/HeadEatEnterFood.png");
    SnakeMouthEatSprites.enteringCell.layer2 = LoadTexture("resources/snakeSprites/HeadEatEnterTop.png");

    SnakeMouthCloseSprites.enteringCell.main = LoadTexture("resources/snakeSprites/HeadEatEnterBottom.png");
    SnakeMouthCloseSprites.enteringCell.layer1 = emptySprite;
    SnakeMouthCloseSprites.enteringCell.layer2 = LoadTexture("resources/snakeSprites/HeadEatEnterTop.png");

    // Snake Mouth Open
    SnakeMouthOpenSprites.enteringCell.main = LoadTexture("resources/snakeSprites/HeadOpenEnter.png");

    SnakeMouthOpenSprites.leavingCell.main = LoadTexture("resources/snakeSprites/HeadOpenLeave.png");
    SnakeMouthOpenSprites.leavingCell.left = LoadTexture("resources/snakeSprites/HeadOpenLeaveLeft.png");
    SnakeMouthOpenSprites.leavingCell.right = LoadTexture("resources/snakeSprites/HeadOpenLeaveRight.png");

    // Snake Body
    SnakeBodySprites.enteringCell.main = LoadTexture("resources/snakeSprites/BodyEnter.png");
    SnakeBodySprites.enteringCell.left = LoadTexture("resources/snakeSprites/BodyEnterLeft.png");
    SnakeBodySprites.enteringCell.right = LoadTexture("resources/snakeSprites/BodyEnterRight.png");

    SnakeBodySprites.leavingCell.main = LoadTexture("resources/snakeSprites/BodyLeave.png");
    SnakeBodySprites.leavingCell.left = LoadTexture("resources/snakeSprites/BodyLeaveLeft.png");
    SnakeBodySprites.leavingCell.right = LoadTexture("resources/snakeSprites/BodyLeaveRight.png");

    // Snake Tail
    SnakeTailSprites.enteringCell.main = LoadTexture("resources/snakeSprites/TailEnter.png");
    SnakeTailSprites.enteringCell.left = LoadTexture("resources/snakeSprites/TailEnterLeft.png");
    SnakeTailSprites.enteringCell.right = LoadTexture("resources/snakeSprites/TailEnterRight.png");

    SnakeTailSprites.leavingCell.main = LoadTexture("resources/snakeSprites/TailLeave.png");
    SnakeTailSprites.leavingCell.left = SnakeTailSprites.leavingCell.main;
    SnakeTailSprites.leavingCell.right = SnakeTailSprites.leavingCell.main;

    // Tail Lengthen
    TailLengthenSprites.enteringCell.main = LoadTexture("resources/snakeSprites/TailLengthen.png");
    TailLengthenSprites.enteringCell.left = LoadTexture("resources/snakeSprites/TailLengthenLeft.png");
    TailLengthenSprites.enteringCell.right = LoadTexture("resources/snakeSprites/TailLengthenRight.png");

    // Food
    FoodSprite = LoadTexture("resources/Food.png");
}
void LoadSounds()
{
    ButtonClick = LoadSound("resources/Sounds/ButtonClick.wav");
    StartGame = LoadSound("resources/Sounds/GameStart.wav");
    WinGame = LoadSound("resources/Sounds/GameWin.wav");
    SwitchScreen = LoadSound("resources/Sounds/SwitchScreen.wav");
    SnakeEat = LoadSound("resources/Sounds/SnakeEat.wav");
    SnakeDeath = LoadSound("resources/Sounds/SnakeDeath.wav");
}
void initGame(Snake *snake, Cell board[boardSize][boardSize])
{
    createBoard(board);
    initSnake(snake, board);
    generateFood(board, snake);
}
void initBoardSizes(int *cellSize, Position *boardStart)
{
    *cellSize = screenHeight / (boardSize);                            // Find the width and the height for each square on the board
    int boardStartX = (screenWidth / 2) - (*cellSize * boardSize / 2); // Calculate where to start drawing the board
    int boardStartY = (screenHeight - *cellSize * boardSize) / 2;      // Calculate where to start drawing the board
    *boardStart = (Position){boardStartX, boardStartY};                // Store the board starting position
}
void setDifficultySettings(int difficulty, float *snakeUpdateBaseInterval)
{
    // Set the speed of the snake and the size of the board based on the users selected difficulty
    if (difficulty == 0) // Easy
    {
        boardSize = 6;
        *snakeUpdateBaseInterval = 0.3f;
    }
    // Medium
    else if (difficulty == 1)
    {
        boardSize = 8;
        *snakeUpdateBaseInterval = 0.2f;
    }
    // Hard
    else if (difficulty == 2)
    {
        boardSize = 12;
        *snakeUpdateBaseInterval = 0.15f;
    }
    // Expert
    else if (difficulty == 3)
    {
        boardSize = 14;
        *snakeUpdateBaseInterval = 0.09f;
    }
    boardSize += 2; // add 2 to board size so there is space for walls
}

void DrawStartScreen(Button *buttons[])
{
    // Start menu
    DrawCenteredText("SNAKE", 40, 90, BLACK, screenWidth);

    // Difficulty buttons
    DrawButton(*buttons[0]);
    DrawButton(*buttons[1]);
    DrawButton(*buttons[2]);
    DrawButton(*buttons[3]);

    DrawCenteredText("PRESS [ENTER] TO START!", 480, 23, BLACK, screenWidth);
}
void DrawBoard(Cell board[boardSize][boardSize], int cellSize, Position boardStart, Snake snake, int screenWidth, float snakeUpdateInterval, int difficulty, int currentFrame)
{
    // Help text
    DrawText("[P] to pause.", 10, 10, 23, BLACK);
    DrawText("[R] to restart.", 10, 40, 23, BLACK);
    DrawText("[W], [A], [S], [D] /", 10, 80, 23, BLACK);
    DrawText("[ARROW KEYS]", 10, 105, 23, BLACK);
    DrawText("to move.", 10, 130, 23, BLACK);

    // Game data
    char scoreText[50];
    sprintf(scoreText, "SCORE: %d", snake.tailIndex - 2);
    int textWidth = MeasureText(scoreText, 35);
    DrawText(scoreText, screenWidth - textWidth - 20, 20, 35, BLACK);
    char difficultyText[50];
    sprintf(difficultyText, "DIFFICULTY: %s", DifficultyToString(difficulty));
    textWidth = MeasureText(difficultyText, 18);
    DrawText(difficultyText, screenWidth - textWidth - 20, 55, 18, BLACK);

    // Loop though all cells in the board drawing them
    for (int i = 0; i < boardSize; i++)
    {
        for (int j = 0; j < boardSize; j++)
        {
            int X = boardStart.x + i * cellSize; // x coordinate for current cell
            int Y = boardStart.y + j * cellSize; // y coordinate for current cell

            // Display cell with diffent colors depending on its contents
            if (board[i][j].contents == BOARDWALL)
            {
                DrawRectangle(X, Y, cellSize, cellSize, DARKGRAY);
                continue; // Skip to next cell
            }
            else
            {
                // Alternate between white and light gray for all cells that arent walls
                if ((i + j) % 2 == 0)
                    DrawRectangle(X, Y, cellSize, cellSize, LIGHTGRAY);
                else
                    DrawRectangle(X, Y, cellSize, cellSize, DARKERLIGHTGRAY);

                if (board[i][j].contents == EMPTY)
                    continue; // Skip to next cell
            }

            // Draw snake based on which part of the snake is in current cell
            if (board[i][j].contents == SNAKEBODY)
            {
                AnimateSprite(X, Y, cellSize, board[i][j].spriteEnteringCell, board[i][j].snakeSpriteDirection, snakeSpriteFrame);
                AnimateSprite(X, Y, cellSize, board[i][j].spriteLeavingCell, board[i][j].snakeSpriteDirectionLeaving, snakeSpriteFrame);

                if (board[i][j].multipleLayers) // Snake mouth eat and close have multiple layers
                {
                    AnimateSprite(X, Y, cellSize, board[i][j].layer1, LEFT, snakeSpriteFrame); // Food doesnt rotate based on snakes direction
                    AnimateSprite(X, Y, cellSize, board[i][j].layer2, board[i][j].snakeSpriteDirection, snakeSpriteFrame);
                }
            }
            if (board[i][j].contents == FOOD)
            {
                AnimateSprite(X, Y, cellSize, FoodSprite, 90, currentFrame);
            }
        }
    }

    // Draw the snake death animation over everything (the snake death animation is 2 cells wide)
    if (gameState == DEATHANIMATION && DeathType == SNAKEBODY)
    {
        int X = boardStart.x + snake.snakeSegments[2]->index.x * cellSize;                                                        // x coordinate for current cell
        int Y = boardStart.y + snake.snakeSegments[2]->index.y * cellSize;                                                        // y coordinate for current cell
        AnimateLongSprite(X, Y, cellSize, SnakeDeathSnakeSprite, snake.snakeSegments[2]->snakeSpriteDirection, snakeSpriteFrame); // When the snake dies to itself the animation overlaps two cells
    }
}
void DrawWinScreen(int difficulty)
{
    DrawCenteredText("YOU COMPLETED \n       SNAKE!", 150, 80, BLACK, screenWidth);
    // Show the difficulty the user played on
    char difficultyText[50];
    sprintf(difficultyText, "ON %s DIFFICULTY", DifficultyToString(difficulty));
    DrawCenteredText(difficultyText, 320, 35, BLACK, screenWidth);

    DrawCenteredText("PRESS [ENTER] TO RE-START!", 440, 23, BLACK, screenWidth);
}
void DrawDeathScreen(int difficulty)
{
    DrawCenteredText("YOU DIED!", 150, 130, BLACK, screenWidth);
    // Show the score the user achived
    char scoreText[50];
    sprintf(scoreText, "YOUR SCORE WAS: %d", scoreAchieved);
    DrawCenteredText(scoreText, 290, 35, BLACK, screenWidth);
    // Show the difficulty the user played on
    char difficultyText[50];
    sprintf(difficultyText, "ON %s DIFFICULTY", DifficultyToString(difficulty));
    DrawCenteredText(difficultyText, 325, 35, BLACK, screenWidth);
    DrawCenteredText("PRESS [ENTER] TO RE-START!", 440, 23, BLACK, screenWidth);
}

void resetGame(Snake *snake, Cell board[boardSize][boardSize], int gameStateToGoTo)
{
    // Reset all variables so game can be played again
    gameState = gameStateToGoTo;
    cleanup(snake);
    initGame(snake, board);
}
void resetTimeVariables(float *lastSnakeUpdateTime)
{
    snakeSpriteFrame = 0;
    *lastSnakeUpdateTime = GetTime();
    pausedStartTime = 0.0f;
    totalPausedTime = 0.0f;
}
bool checkWin(Snake *snake, Cell board[boardSize][boardSize])
{
    // Returns if the player has won and if they have then also resets the game
    if (numCellsToFill(snake, board) == 0) // If the snake has filled all the cells
    {
        PlaySound(WinGame);
        resetGame(snake, board, WINSCREEN); // Restart the game
        return true;
    }
    return false;
}

void menuInputs(bool *paused, Snake *snake, Cell board[boardSize][boardSize], float *lastSnakeUpdateTime)
{
    if (IsKeyPressed(KEY_ENTER)) // Start game
    {
        if (gameState == STARTMENU) // If on start menu go to game when ENTER is hit
        {
            PlaySound(StartGame);
            resetTimeVariables(lastSnakeUpdateTime);
            gameState = GAME;
        }
        else if (gameState == WINSCREEN || gameState == DEATHSCREEN) // If on win or death screen go to start menu when ENTER is hit
        {
            PlaySound(SwitchScreen);
            gameState = STARTMENU;
        }
        *paused = false; // Always unpause when pressing ENTER
    }
    if (IsKeyPressed(KEY_P)) // Pause game
    {
        PlaySound(SwitchScreen);
        *paused = !(*paused); // Toggle Pause
    }
    if (IsKeyPressed(KEY_R)) // Reset game
    {
        PlaySound(SwitchScreen);
        resetGame(snake, board, STARTMENU);
    }
}
void pausedTimer(bool paused)
{
    // Records the amount of time the game has been paused for so animations dont jump when unpausing
    if (paused)
    {
        if (pausedStartTime == 0.0f) // Start of pause
            pausedStartTime = GetTime();
    }
    else if (pausedStartTime != 0.0f) // Resuming from pause
    {
        totalPausedTime += GetTime() - pausedStartTime; // Keeps updating how long the game has been paused for
        pausedStartTime = 0.0f;
    }
}
void buttonInputs(Button *buttons[], int buttonCount, int *difficulty, float *snakeUpdateBaseInterval, Cell board[boardSize][boardSize], Snake *snake, int *cellSize, Position *boardStart)
{
    // Loop though each button in the list 'buttons'
    for (int i = 0; i < buttonCount; i++)
    {
        CheckButtonHover(buttons[i]);                                                  // Update hover state for current button
        if (IsMouseOverButton(*buttons[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // If clicking on the button
        {
            PlaySound(ButtonClick);
            // Deselect all buttons
            for (int j = 0; j < buttonCount; j++)
            {
                buttons[j]->selected = false;
            }
            // Set the current button as selected
            buttons[i]->selected = true;
            // Set the difficulty level to the index of the current button
            *difficulty = i;
            setDifficultySettings(*difficulty, snakeUpdateBaseInterval); // Apply the difficulty settings to the game
            // Re-initialise board and snake to apply new dificulty settings
            initBoardSizes(cellSize, boardStart);
            initGame(snake, board);
        }
    }
}
void playerInputs(Snake *snake, Cell board[boardSize][boardSize])
{
    int currentDirection = snake->head.snakeDir; // Store the direction of the snake and dont accept inputs that would make the snake go back on itself
    int *inputDirection = &snake->head.headDir;  // Create a pointer to the variable that holdes direction of the head for the next snake move

    if (snake->head.headDir != NOTSET) // If the player has already made an input for the head direction fill the head direction buffer instead
    {
        currentDirection = snake->head.headDir;      // Set the direction of the snake to what it will be after the next move to make sure the move in the buffer is valid
        inputDirection = &snake->head.headDirBuffer; // set the pointer to reference the bufferDir instead of the headDir
    }

    // Runs with snake direction limiting the vaild moves if 'headDir' not yet set. Else runs with 'headDir' as the direction limiter
    // Runs with 'headDir' as the direction being changed if it hasent been set yet. Else runs with 'headDirBuffer' as the direction being stored
    if (currentDirection != DOWN && currentDirection != UP) // Only allow up and down inputs if the snakes direction is left or right
    {
        if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)))
        {
            *inputDirection = UP; // Set the direction the snake will take when it next moves to UP
        }
        if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)))
        {
            *inputDirection = DOWN; // Set the direction the snake will take when it next moves to DOWN
        }
    }
    else if (currentDirection != LEFT && currentDirection != RIGHT) // Only allow left and right inputs if the snakes direction is up or down
    {
        if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)))
        {
            *inputDirection = LEFT; // Set the direction the snake will take when it next moves to LEFT
        }
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)))
        {
            *inputDirection = RIGHT; // Set the direction the snake will take when it next moves to RIGHT
        }
    }

    // If there is a move in 'headDir' set the head segement to the correct cell based on the cell adjacent snakes front in the specified direction
    if (snake->head.headDir != NOTSET)
    {
        Position snakeFrontPos = snake->head.snakeFront;                                   // Find the snakes front cell index
        Position newSnakeHeadPos = getNextCellFromDir(snakeFrontPos, snake->head.headDir); // Find the index for where the head will go based on the input direction
        snake->snakeSegments[0] = &board[newSnakeHeadPos.x][newSnakeHeadPos.y];            // Set the head pointer to its new board cell
    }
}

void SetSnakesMouthState()
{
    // Updates snake mouth state based on previous mouth state (updates every snake move)
    if (snakeMouthState == OPENING)
        snakeMouthState = CLOSING;
    else if (snakeMouthState == CLOSING || snakeMouthState == EATING)
        snakeMouthState = CLOSED;
}
void CheckSnakeDeath(Snake *snake)
{
    // If snake has hit a wall or itself
    if (snake->snakeSegments[0]->contents == SNAKEBODY || snake->snakeSegments[0]->contents == BOARDWALL)
    {
        if (snake->snakeSegments[0] != snake->snakeSegments[snake->tailIndex - 2] && snake->snakeSegments[0] != snake->snakeSegments[snake->tailIndex - 1]) // Dont end the game if the head is about to bite the tail that will be removed
        {
            PlaySound(SnakeDeath);
            scoreAchieved = snake->tailIndex - 2;          // Calculate score from snakes length
            gameState = DEATHANIMATION;                    // Set to display snake dying
            DeathType = snake->snakeSegments[0]->contents; // Death type equals the contents of the cell the snake hit (BOARDWALL or SNAKEBODY)
        }
    }
}
void CheckSnakeEat(Snake *snake, Cell board[boardSize][boardSize])
{
    if (snake->snakeSegments[0]->contents == FOOD) // Snake has eaten food
    {
        PlaySound(SnakeEat);
        snake->tailIndex++; // Increse length by one
        if (checkWin(snake, board))
            return; // Doesnt continue if player has won
        else
            generateFood(board, snake); // Create new food in random vaild location
        snakeMouthState = EATING;       // Set mouth to eating
    }
    if (isAdjacentToFood(snake->snakeSegments[1], board) && snakeMouthState != EATING) // If near food open mouth ready to eat or close
        snakeMouthState = OPENING;
}
void ChooseMouthSprite(Snake *snake)
{
    // Selects which mouth sprite to used based off the snakes mouth state
    if (snakeMouthState == OPENING)
        SetEnteringAndLeavingSpriteNoDir(SnakeMouthOpenSprites, EmptyCellSprites, snake->snakeSegments[1]);
    else if (snakeMouthState == EATING)
    {
        SetEnteringAndLeavingSpriteNoDir(SnakeMouthEatSprites, EmptyCellSprites, snake->snakeSegments[1]);
        snake->snakeSegments[1]->multipleLayers = true;                             // Multiple layers need to be drawn
        snake->snakeSegments[1]->layer1 = SnakeMouthEatSprites.enteringCell.layer1; // Food
        snake->snakeSegments[1]->layer2 = SnakeMouthEatSprites.enteringCell.layer2;
    }
    else if (snakeMouthState == CLOSING)
    {
        SetEnteringAndLeavingSpriteNoDir(SnakeMouthCloseSprites, EmptyCellSprites, snake->snakeSegments[1]);
        snake->snakeSegments[1]->multipleLayers = true;                               // Multiple layers need to be drawn
        snake->snakeSegments[1]->layer1 = SnakeMouthCloseSprites.enteringCell.layer1; // Empty
        snake->snakeSegments[1]->layer2 = SnakeMouthCloseSprites.enteringCell.layer2;
    }
    else
        SetEnteringAndLeavingSpriteNoDir(SnakeHeadSprites, EmptyCellSprites, snake->snakeSegments[1]);
}

void setAnimationFrame(float lastSnakeUpdateTime, float snakeUpdateInterval)
{
    float currentTime = GetTime(); // Get the current time since the program started

    // Time since snake moved divided by how often the snake moves multiplied by the number of snake frames
    snakeSpriteFrame = (currentTime - (lastSnakeUpdateTime + totalPausedTime)) / snakeUpdateInterval * 5;
    if (snakeSpriteFrame > 4 && gameState != DEATHANIMATION) // Dont reset to frame 0 if displaying the snakes death animation (so the game knows when the death animation is over)
        snakeSpriteFrame = 0;
}
void moveSnake(float *lastSnakeUpdateTime, float snakeUpdateBaseInterval, float *snakeUpdateInterval, float speedIncreasePerSegement, Cell board[boardSize][boardSize], Snake *snake)
{
    float currentTime = GetTime();                                                                        // Get the current time since the program started
    *snakeUpdateInterval = snakeUpdateBaseInterval - (speedIncreasePerSegement * (snake->tailIndex - 2)); // Update snake speed based on the number of segements currently in the snake

    // check if it is time to move the snake
    if (currentTime - (*lastSnakeUpdateTime + totalPausedTime) >= *snakeUpdateInterval) // Add pause time to last update time to account for time spent in pause menu
    {
        SetSnakesMouthState(); // Update mouth state

        if (snake->head.headDir != NOTSET)              // If the user has inputed a new direction for the snake (snake needs to turn this move)
            snake->head.snakeDir = snake->head.headDir; // Set head state so snake cant move back on itself next move

        // If snake has hit a wall or itself
        CheckSnakeDeath(snake);

        // Move snake by moving the pointers to the board cells down snake list
        for (int i = snake->tailIndex; i > 0; i--) // Loop through snake staring from tail (dont do index 0 so head is not effected)
        {
            snake->snakeSegments[i] = snake->snakeSegments[i - 1]; // Move pointers to cells down snake list
        }

        if (snake->snakeSegments[snake->tailIndex] != NULL)
        {
            tailPointDirection = snake->snakeSegments[snake->tailIndex]->snakeSpriteDirectionLeaving; // Store the direction the tail is pointing in
            snake->snakeSegments[snake->tailIndex]->contents = EMPTY;                                 // Set last body section of the snake to empty on the board (Removing the tail)
            snake->snakeSegments[snake->tailIndex] = NULL;                                            // Remove last body section of the snake (tail) moving the snake and keeping the length constant
        }
        // Checks if the snake is on a food cell and sets the mouth state to opening if adjecent to food cell
        CheckSnakeEat(snake, board);

        // Set new snake front X and Y position indexes to be stored in the snake head
        snake->head.snakeFront = findPos(snake->snakeSegments[1]);

        // Get new snake head based on the direction the head is facing (got from the last player input (snakeDir))
        // (will be changed if player makes an input before next move)
        Position nextHeadPos = getNextCellFromDir(snake->head.snakeFront, snake->head.snakeDir);
        snake->snakeSegments[0] = &board[nextHeadPos.x][nextHeadPos.y]; // Set new snake head

        snake->head.headDir = snake->head.headDirBuffer; // Set the direction for next move to what is in the buffer (usually 'NOTSET')
        snake->head.headDirBuffer = NOTSET;              // Reset the buffer

        *lastSnakeUpdateTime = currentTime - totalPausedTime; // Set the last update time
    }
}
void updateBoardForSnake(Snake *snake)
{
    if (gameState != DEATHANIMATION)
        snake->snakeSegments[1]->snakeSpriteDirection = snake->head.snakeDir; // Store the direction in which the snake is entering the cell
    else
        snake->snakeSegments[1] = NULL;
    snake->snakeSegments[2]->snakeSpriteDirection = snake->head.snakeDir;                                 // Update the direction the snake is entering the cell
    snake->snakeSegments[2]->snakeSpriteDirectionLeaving = snake->snakeSegments[2]->snakeSpriteDirection; // Store the direction the snake was entering the cell in (used if the snakes head is entering the same cell as the tail)

    // Update board to show the snake
    for (int i = 1; i < snake->tailIndex; i++) // Dont add body sprites for the snakes head (i = 0)
    {
        if (snake->snakeSegments[i] != NULL) // Only modify segments that are currently part of the snake
        {
            snake->snakeSegments[i]->contents = SNAKEBODY;                             // Set the refrenced board cell to display the snake body segment
            snake->snakeSegments[i]->multipleLayers = false;                           // Reset to false by default (only set to true when drawing snake mouth eating and closing)
            SetEnteringAndLeavingSprite(SnakeBodySprites, SnakeBodySprites, snake, i); // Set the entering and leaving cell sprite to snake body with correct turn direction
        }
    }

    // Set head entering and leaving sprite
    if (gameState != DEATHANIMATION)
    {
        ChooseMouthSprite(snake); // Select mouth sprite
        SetLeavingSprite(SnakeHeadSprites, snake, 2);
    }
    else // If dying play head dying animation
    {
        if (DeathType == SNAKEBODY)
            snake->snakeSegments[2]->spriteLeavingCell = EmptyCellSprites.leavingCell.main;
        else
            snake->snakeSegments[2]->spriteLeavingCell = SnakeDeathWallSprite;
    }

    // Set tail entering and leaving sprite
    if (snakeMouthState == EATING) // Set tail lengthening sprite if eating
        SetEnteringAndLeavingSpriteWithDir(TailLengthenSprites, EmptyCellSprites, snake->snakeSegments[snake->tailIndex - 2], tailPointDirection);
    else
        SetEnteringSprite(SnakeTailSprites, snake, snake->tailIndex - 2);

    if (snake->snakeSegments[snake->tailIndex - 1] != NULL)
    {
        if (snake->snakeSegments[1] != NULL && isSameCell(*snake->snakeSegments[1], *snake->snakeSegments[snake->tailIndex - 1])) // Dont set entering sprite to empty if head is entering tails cell
            SetLeavingSprite(SnakeTailSprites, snake, snake->tailIndex - 1);
        else
            SetEnteringAndLeavingSpriteNoDir(EmptyCellSprites, SnakeTailSprites, snake->snakeSegments[snake->tailIndex - 1]);
    }
}

int main(void)
{
    srand(time(0)); // Use current time to seed random number generator

    int difficulty = 1;                           // Store difficulty level (0-3) (default to medium: 1)
    const int buttonStartPosX = 250;              // How far fron the left the difficulty buttons start
    const int buttonStartPosY = screenHeight / 2; // Buttons display halfway down the screen
    Button buttonEasy = {false, false, {buttonStartPosX, buttonStartPosY, 120, 70}, "Easy"};
    Button buttonMedium = {false, true, {buttonStartPosX + 130, buttonStartPosY, 120, 70}, "Medium"}; // Set medium to default selected
    Button buttonHard = {false, false, {buttonStartPosX + 260, buttonStartPosY, 120, 70}, "Hard"};
    Button buttonExpert = {false, false, {buttonStartPosX + 390, buttonStartPosY, 120, 70}, "Expert"};
    Button *buttons[] = {&buttonEasy, &buttonMedium, &buttonHard, &buttonExpert}; // Create a list of the buttons
    const int buttonCount = 4;                                                    // Number of buttons (used for for loops)

    bool paused = false;              // Is game paused
    boardSize = 16;                   // Size of the playable board (set to 16 witch is the biggest possible on expert difficulty)
    boardSize += 2;                   // Add 2 to board size so there is space for walls
    Cell board[boardSize][boardSize]; // Create a 2d array with the size of the biggest possible board

    int cellSize;        // Store the width and the height for each square on the board
    Position boardStart; // Store where to start drawing the board

    Snake snake;
    snake.snakeSegments = NULL;
    float snakeUpdateBaseInterval;           // Base speed without speed increses when snake lengthens
    float snakeUpdateInterval;               // Current speed of the snake ('snakeUpdateBaseInterval' + snake length * 'speedIncreasePerSegement')
    float speedIncreasePerSegement = 0.001f; // How much the snake should speed up per segement
    float lastSnakeUpdateTime = 0.0f;        // Time since last snake movement

    int currentFrame = 0;  // Current frame of sprite animations (used for animating food)
    int framesCounter = 0; // Count frames elapsed since adding 1 to current frame
    int framesSpeed = 7;   // Number of spritesheet frames shown by second

    setDifficultySettings(difficulty, &snakeUpdateBaseInterval); // Set game settings based on selected difficulty
    initBoardSizes(&cellSize, &boardStart);                      // Initialise the board
    initGame(&snake, board);                                     // Initialise the snake

    // init main window
    InitWindow(screenWidth, screenHeight, "Snake"); // Start window
    InitAudioDevice();                              // Start audio
    SetTargetFPS(60);

    LoadSprites(); // Load animation sprites
    LoadSounds();  // Load sound

    while (!WindowShouldClose())
    {
        framesCounter++; // Count frames for animations

        // Increase current frame if more than x amount of frames have elapsed
        if (framesCounter >= (60 / framesSpeed))
        {
            framesCounter = 0; // Reset frame counter
            currentFrame++;    // Increase animation frame

            if (currentFrame > 4) // Loop animation frame round when over maximum (there are only 5 frames in each sprite)
                currentFrame = 0;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE); // Clear screen

        // Get menu keyboard inputs (start game, pause, resart)
        menuInputs(&paused, &snake, board, &lastSnakeUpdateTime);
        pausedTimer(paused); // Record time spent in the pause menu so game doesnt jump forward when unpausing

        if (gameState == GAME) // If game has started
        {
            if (!paused) // If not paused run game
            {
                playerInputs(&snake, board);                                                                                             // Direction inputs
                setAnimationFrame(lastSnakeUpdateTime, snakeUpdateInterval);                                                             // Update the animation frame the snakeis on
                moveSnake(&lastSnakeUpdateTime, snakeUpdateBaseInterval, &snakeUpdateInterval, speedIncreasePerSegement, board, &snake); // Move snake at intervals based on snake speed
                updateBoardForSnake(&snake);                                                                                             // Modify the boards cells to store information about the snake
            }

            // Draw game board
            DrawBoard(board, cellSize, boardStart, snake, screenWidth, snakeUpdateInterval, difficulty, currentFrame);
            if (paused)
            {
                // Show game is paused
                DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 50}); // Semi-transparent black covering the whole screen
                DrawCenteredText("PAUSED!", screenHeight / 2, 23, WHITE, screenWidth);
            }
        }
        else if (gameState == DEATHANIMATION) // For the next move after the snake dies display death animation
        {
            setAnimationFrame(lastSnakeUpdateTime, snakeUpdateInterval); // Keep updating the animation frame for the death animation
            if (snakeSpriteFrame > 4)                                    // Once animation has completed switch to death screen
                resetGame(&snake, board, DEATHSCREEN);
            DrawBoard(board, cellSize, boardStart, snake, screenWidth, snakeUpdateInterval, difficulty, currentFrame); // Draw board
        }
        else if (gameState == STARTMENU) // If not started draw start screen
        {
            // Update button states (selected, hovered) and check for clicks
            buttonInputs(buttons, buttonCount, &difficulty, &snakeUpdateBaseInterval, board, &snake, &cellSize, &boardStart);
            DrawStartScreen(buttons);
        }
        else if (gameState == WINSCREEN)
        {
            DrawWinScreen(difficulty);
        }
        else if (gameState == DEATHSCREEN) // After death animation draw death screen
        {
            DrawDeathScreen(difficulty);
        }

        EndDrawing();
    }
    // clear up and shut down
    cleanup(&snake);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}