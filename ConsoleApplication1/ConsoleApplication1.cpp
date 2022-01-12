#include "Includes.h"
#include "Tetromino.h"
#include <ctime>
#include "Globals.h"
#pragma comment(lib,"x64\\SDL2.lib")
#pragma comment(lib,"x64\\SDL2_ttf.lib")

// the pieces stored in ints
// 0 = blank space
// 1 = cyan
// 2 = blue
// 3 = orange
// 4 = yellow
// 5 = green
// 6 = purple
// 7 = red

const int tetrominos[7][8] = { {
	{1},{1},{1},{1}, // 4 Wide
	{0},{0},{0},{0}
	},
	{
	{2},{0},{0},{0}, // L-Piece
	{2},{2},{2},{0}
	},
	{
	{0},{0},{3},{0}, // Reverse L-Piece
	{3},{3},{3},{0}
	},
	{
	{4},{4},{0},{0}, // Square
	{4},{4},{0},{0}
	},
	{
	{0},{5},{5},{0}, // Reverse S
	{5},{5},{0},{0}
	},
	{
	{0},{6},{0},{0}, // T-Piece
	{6},{6},{6},{0}
	},
	{
	{7},{7},{0},{0}, // S
	{0},{7},{7},{0}
	}
};



// [level][0] = speed for that level

const float cellFrames[16][1] = { // gotten from https://gamedev.stackexchange.com/questions/159835/understanding-tetris-speed-curve
	{0.01667},
	{0.021017},
	{0.026977},
	{0.035256},
	{0.04693},
	{0.06361},
	{0.0879},
	{0.1236},
	{0.1775},
	{0.2598},
	{0.388},
	{0.59},
	{0.92},
	{1.46},
	{2.36},
};

float deltaTime = 0;

std::vector<TetrominoPiece> groundedPieces[21];

Tetromino* faillingTetr;
Tetromino* ghostTetr;

bool isOffseting = false;

int selectedPieceIndex = 0;

std::vector<int> offsetX;
std::vector<int> offsetY;

float boardX = ((864 / 2) - 80);
float bottomBoard = 512;

Tetromino* held = NULL;
bool usedHold = false;

void holdPiece()
{
	if (!usedHold)
	{
		if (held == NULL)
		{
			held = faillingTetr;
			faillingTetr = nullptr;
			usedHold = true;
		}
		else
		{
			faillingTetr = held;
			held = NULL;
			usedHold = true;
		}
	}
}

void createTetr(int constructId) {
	usedHold = false;
	Tetromino* tetr = new Tetromino();

	tetr->constructIndex = constructId;
	tetr->rect.w = 16;
	tetr->rect.h = 16;

	int colIndex = 0;
	int lanes = 1;

	for (int ii = 0; ii < 4; ii++)
	{
		int piece = tetrominos[constructId][ii];
		int bottomPiece = tetrominos[constructId][ii + 4];
		if (piece != 0)
			tetr->addPiece(0, colIndex, piece);
		if (bottomPiece != 0)
		{
			tetr->addPiece(1, colIndex, bottomPiece);
			lanes = 2;
		}

		if (piece != 0 || bottomPiece != 0)
			colIndex++;

	}
	tetr->rect.w = colIndex * 16;
	tetr->rect.h = lanes * 16;
	tetr->rect.x = boardX + (16 * 3);
	tetr->rect.y = 192;
	tetr->storeY = 192;
	faillingTetr = tetr;
}


void toClipboard(const std::string& s) {
	OpenClipboard(0);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size());
	if (!hg) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hg), s.c_str(), s.size());
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
}

float currentTime = 0;
float simulatedTime = 0;


int level;


float align(float value, float size)
{
	return value - std::abs(std::fmod(value, size));
}


void checkBoardCol() {
	for (TetrominoPiece piece : faillingTetr->pieces)
	{
		if (faillingTetr->rect.x + piece.rect.x < boardX)
			faillingTetr->rect.x += 16;
		if (faillingTetr->rect.x + piece.rect.x >= boardX + 160)
			faillingTetr->rect.x -= 16;
	}

}

int findLaneOfPiece(int pieceY) {
	int lane = pieceY - 192;
	for (int i = 0; i < 21; i++)
	{
		int laneMay = i * 16;
		if (lane == laneMay)
			return i;
	}
	return 0;
}

Tetromino* lowestPos() {
	Tetromino* copy = new Tetromino();
	copy->pieces = faillingTetr->pieces;
	copy->rect = faillingTetr->rect;
	bool landed = false;
	TetrominoPiece lowestPiece = copy->pieces[0];
	for (TetrominoPiece p : copy->pieces)
	{
		if (p.rect.y > lowestPiece.rect.y)
		{
			lowestPiece = p;
		}
	}
	while (!landed)
	{
		copy->rect.y += 16;

		for (TetrominoPiece p : copy->pieces)
		{	
			int lane = findLaneOfPiece(copy->rect.y + p.rect.y);
			for (TetrominoPiece pp : groundedPieces[lane])
			{
				if (copy->checkCol(pp))
				{
					landed = true;
					copy->rect.y -= 16;
					break;
				}
			}
			if (landed)
				break;
		}

		
		if (copy->rect.y + lowestPiece.rect.y >= bottomBoard && !landed)
		{
			landed = true;
			copy->rect.y -= 16;
		}
	}
	return copy;
}

void resortLanes()
{
	std::vector<TetrominoPiece> pieces;
	for(int lane = 0; lane < 21; lane++)
	{
		for(int i = 0; i < groundedPieces[lane].size(); i++)
		{
			TetrominoPiece piece = groundedPieces[lane][i]; // copy
			pieces.push_back(piece);
		}
		groundedPieces[lane].clear();
	}
	for(int i = 0; i < pieces.size(); i++)
	{
		TetrominoPiece p = pieces[i]; // copy
		groundedPieces[findLaneOfPiece(p.rect.y)].push_back(p);
	}
}

void placePiece()
{
	for (TetrominoPiece piece : faillingTetr->pieces)
	{
		piece.rect.x = faillingTetr->rect.x + piece.rect.x;
		piece.rect.y = faillingTetr->rect.y + piece.rect.y;

		groundedPieces[findLaneOfPiece(piece.rect.y)].push_back(piece);
	}
	// check for line clears
	for(TetrominoPiece piece : faillingTetr->pieces)
	{
		piece.rect.x = faillingTetr->rect.x + piece.rect.x;
		piece.rect.y = faillingTetr->rect.y + piece.rect.y;
		int lane = findLaneOfPiece(piece.rect.y);
		std::vector<TetrominoPiece>& p = groundedPieces[lane];
		std::cout << "Lane " << lane << " pieces: " << p.size() << std::endl;
		if (p.size() == 10)
		{
			std::cout << "Cleared that lane" << std::endl;
			p.clear();
			for(int i = 0; i < lane; i++)
			{
				for(int pi = 0; pi < groundedPieces[i].size(); pi++)
				{
					groundedPieces[i][pi].rect.y += 16;
				}
			}
			resortLanes();
		}
	}
}

int WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PSTR lpCmdLine,
	INT nCmdShow)
{
	srand((unsigned)time(0));
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");


	SDL_Window* window = SDL_CreateWindow("Da window", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, 864, 672, SDL_WINDOW_SHOWN);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	Globals::renderer = renderer;

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	bool run = true;

	createTetr(rand() % 6);

	bool skip = false;
	
	while (run)
	{
		const Uint32 startTime = SDL_GetTicks();
		SDL_RenderClear(renderer);
		SDL_Event event;


		while (SDL_PollEvent(&event) > 0)
		{
			switch (event.type) {
				case SDL_QUIT:
					run = false;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)
					{
					case SDLK_UP:
						if (faillingTetr)
						{
							faillingTetr->rotate();
							checkBoardCol();
						}
						break;
					case SDLK_LEFT:
						if (faillingTetr)
						{
							faillingTetr->rect.x -= 16;
							checkBoardCol();
						}
						break;
					case SDLK_RIGHT:
						if (faillingTetr)
						{
							faillingTetr->rect.x += 16;
							checkBoardCol();
						}
						break;
					case SDLK_DOWN:
						skip = true;
						break;
					case SDLK_c:
						holdPiece();
						break;
					case SDLK_SPACE:
						Tetromino* lowestPiece = lowestPos();

						faillingTetr->rect.y = lowestPiece->rect.y;

						delete lowestPiece;

						if (faillingTetr)
						{
							std::cout << "place" << std::endl;
							placePiece();

							delete faillingTetr;
							faillingTetr = nullptr;
						}
						break;
					}
					break;
				case SDL_KEYUP:
					switch (event.key.keysym.sym)
					{
					case SDLK_DOWN:
						skip = false;
						break;
					}
					break;
			}
		

		}

		currentTime += deltaTime;


		if (faillingTetr)
		{
			ghostTetr = lowestPos();
			
			for(int i = 0; i < ghostTetr->pieces.size(); i++)
			{
				TetrominoPiece& piece = ghostTetr->pieces[i];
				piece.color.r = 255;
				piece.color.g = 255;
				piece.color.b = 255;
				piece.color.a = 128;
			}

			// collision

			bool die = false;

			for (int i = 0; i < 21; i++)
			{
				for (TetrominoPiece& piece : groundedPieces[i])
				{
					if (faillingTetr->checkCol(piece))
					{
						faillingTetr->rect.y -= 16;
						placePiece();
						die = true;
						break;
					}
				}
			}


			for (TetrominoPiece piece : faillingTetr->pieces)
			{
				if (faillingTetr->rect.y + piece.rect.y >= bottomBoard - 16)
				{
					placePiece();
					faillingTetr->rect.y = bottomBoard - 16;
					die = true;
					break;
				}
			}

			if (die)
			{
				delete faillingTetr;
				faillingTetr = nullptr;
			}
			if (faillingTetr)
			{
				if (!skip)
					while (currentTime > simulatedTime)
					{
						faillingTetr->storeY++;
						faillingTetr->rect.y = align(faillingTetr->storeY, 16);

						simulatedTime += 1 / cellFrames[level][0];
					}

				if (skip && SDL_GetTicks() % 10 == 0)
				{
					faillingTetr->storeY++;
					faillingTetr->rect.y = align(faillingTetr->storeY, 16);
				}
			
				faillingTetr->draw();
				ghostTetr->draw();
			}

		}
		else
		{
			createTetr(rand() % 6);
		}

		for (int i = 0; i < 21; i++)
		{
			for (TetrominoPiece& piece : groundedPieces[i])
			{
				SDL_SetRenderDrawColor(Globals::renderer, piece.color.r, piece.color.g, piece.color.b, piece.color.a);
				SDL_FRect newRect;
				newRect.x = piece.rect.x;
				newRect.y = piece.rect.y;
				newRect.w = piece.rect.w;
				newRect.h = piece.rect.h;
				SDL_RenderFillRectF(Globals::renderer, &newRect);
				SDL_SetRenderDrawColor(Globals::renderer, 0, 0, 0, 255);
				SDL_RenderDrawRectF(Globals::renderer, &newRect);
			}
		}
		// render box

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

		SDL_FPoint p[4];

		p[0].x = boardX;
		p[0].y = 192;
		p[1].x = boardX;
		p[1].y = bottomBoard;
		p[2].x = boardX + 160;
		p[2].y = bottomBoard;
		p[3].x = boardX + 160;
		p[3].y = 192;

		SDL_RenderDrawLinesF(renderer, p, 4);


		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderPresent(renderer);

		deltaTime = SDL_GetTicks() - startTime;

	}

	return 0;

}
