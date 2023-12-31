#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>

#include "src/a_star.h"
#include "src/grid.h"
#include "src/heap.h"
#include "src/linked_list.h"

#define WIDTH       1200
#define HEIGHT      800
#define CELL_DIM    20

COLOR VOID_CLR  = {  0,   0,   0};
COLOR WALL_CLR  = {200, 200, 200};
COLOR START_CLR = {  0, 128,   0};
COLOR DEST_CLR  = {128,   0,   0};
COLOR PATH_CLR  = { 10, 120, 255};

enum Mode {WALL_MODE, SRC_MODE, DEST_MODE};
COLOR *main_colors[4] = {&WALL_CLR, &START_CLR, &DEST_CLR, &VOID_CLR};

SDL_Window   *main_win    = NULL;
SDL_Renderer *main_render = NULL;
SDL_Event    evt;

GRID *grid;
int s_pos[] = {-1, -1};
int d_pos[] = {-1, -1};

// Matrix to Linear index
unsigned mat2list( unsigned i, unsigned j, unsigned width );

// Create graph, use A*. Perform animation in the meanwhile
void find_path (GRID *grid);

// Reset inital state of the grid before animation
void restore_grid (GRID *grid);

// Utility: Check whether cell is a wall
int is_wall(GRID *grid, int i, int j);

int main(int argc, char *argv[])
{
    enum Mode mode = WALL_MODE;

    // Initialize Window and Renderer
    if (SDL_Init(SDL_INIT_EVERYTHING)) return -1;
    main_win = SDL_CreateWindow("Main",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, 0
    );
    main_render = SDL_CreateRenderer(main_win, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(main_render, WIDTH, HEIGHT);

    bool is_white = argc == 2 && argv[1][0] == '1';
    COLOR default_clr = (is_white) ? WALL_CLR : VOID_CLR;
    grid = grid_init(0, 0, WIDTH, HEIGHT, CELL_DIM, default_clr);

    // There is a 2nd argument but it's not FILL white,
    // hence we assume the second arg is a filepath
    if (argc == 2 && !is_white) grid_load(argv[1], grid, &WALL_CLR);

    bool can_loop = true;
    COLOR curr;

    int x, y;
    unsigned cellX = WIDTH  / CELL_DIM;
    unsigned cellY = HEIGHT / CELL_DIM;

    // Main_Loop
    while ( can_loop ) {
        while ( SDL_PollEvent(&evt) ) {
            if ( evt.type == SDL_QUIT ) can_loop = false;
            else if ( evt.type == SDL_KEYUP ) {
                switch (evt.key.keysym.sym) {
                    case SDLK_q: can_loop = false;
                    case SDLK_s:
                    case SDLK_b: mode = SRC_MODE; break;
                    case SDLK_e: 
                    case SDLK_d: mode = DEST_MODE;  break;
                    case SDLK_w: mode = WALL_MODE;  break;

                    case SDLK_p: grid_save(grid, "saved_grid.txt", &WALL_CLR); break;
                    case SDLK_l: grid_load("saved_grid.txt", grid, &WALL_CLR); break;

                    // On Enter: start / stop search
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (s_pos[0] == -1) continue;
                        curr = grid -> cells[s_pos[1]][s_pos[0]] -> clr;

                        // To check whether we have to start or stop,
                        // we look at the color of the Start position:
                        // if still START_CLR, we have to start; else we reset
                        if ( curr.r == START_CLR.r && curr.g == START_CLR.g && curr.b == START_CLR.b)
                            find_path (grid);
                        else restore_grid(grid);
                    break;
                };

            // Draw cell on mouse click.
            // Also supports mouse move to draw multiple cells
            } else if ( evt.type == SDL_MOUSEBUTTONDOWN || evt.type == SDL_MOUSEMOTION ) {
                x = evt.button.x / CELL_DIM;
                y = evt.button.y / CELL_DIM;

                if (x < 0 || y < 0) continue;
                if (x >= cellX || y >= cellY) continue;

                bool is_left = (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) ||
                               (evt.type == SDL_MOUSEMOTION     && (evt.motion.state & SDL_BUTTON_LMASK));

                bool is_right = (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button==SDL_BUTTON_RIGHT) ||
                                (evt.type == SDL_MOUSEMOTION     && (evt.motion.state & SDL_BUTTON_RMASK));

                if (is_left) {
                    if (mode != WALL_MODE)
                    {
                        // Select either start or destination coordinates array
                        int *curr_coords = (mode == SRC_MODE) ? s_pos : d_pos;

                        // If a position was already set, we need to
                        // remove the previous one from the grid
                        if (curr_coords[0] != -1)
                        {
                            CELL *prev_cell = grid -> cells[curr_coords[1]][curr_coords[0]];
                            prev_cell -> clr.r = VOID_CLR.r;
                            prev_cell -> clr.g = VOID_CLR.g;
                            prev_cell -> clr.b = VOID_CLR.b;
                        }

                        // Set new coords
                        curr_coords[0] = x;
                        curr_coords[1] = y;
                    }

                    // Draw new coord
                    grid -> cells[y][x] -> clr.r = main_colors[mode] -> r;
                    grid -> cells[y][x] -> clr.g = main_colors[mode] -> g;
                    grid -> cells[y][x] -> clr.b = main_colors[mode] -> b;

                } else if (is_right) {
                    grid -> cells[y][x] -> clr.r = main_colors[3] -> r;
                    grid -> cells[y][x] -> clr.g = main_colors[3] -> g;
                    grid -> cells[y][x] -> clr.b = main_colors[3] -> b;
                }

            }
        }

        SDL_SetRenderDrawColor(main_render, 128, 128, 128, 255);
        SDL_RenderClear(main_render);
        grid_draw(main_render, grid);
        SDL_Delay(100);
    }

    SDL_DestroyRenderer(main_render);
    SDL_DestroyWindow(main_win);
    main_render = NULL;
    main_render = NULL;
    SDL_Quit();

    grid_destroy(grid, WIDTH / CELL_DIM, HEIGHT / CELL_DIM);

    return 0;
} 

void find_path ( GRID *grid ) {
    unsigned CELLX = grid -> CELLX;
    unsigned CELLY = grid -> CELLY;
    unsigned n_verts = CELLX * CELLY;
    unsigned n_edges = 0;

    A_Vertex *verts[n_verts] = {};
    A_Vertex *new_vert = NULL;
    A_Edge *new_edges = NULL;

    // If Start and Destination not set, nothing to do
    if (s_pos[0] == -1 || d_pos[0] == -1) return;

    char offsets[5] = {0, -1, 0, 1, 0};
    for (int i = 0; i < grid -> CELLY; i++)
    for (int j = 0; j < grid -> CELLX; j++)
    {
        if (is_wall(grid, i, j)) continue;

        // Each node can have up to 4 neighbors
        new_edges = malloc(4 * sizeof(A_Edge));
        n_edges = 0;
        for (int k = 0; k < 4; ++k)
        {
            int sub_i = i + offsets[k];
            int sub_j = j + offsets[k+1];

            if (sub_i < 0 || sub_i > CELLY -1) continue;
            if (sub_j < 0 || sub_j > CELLX -1) continue;
            if (sub_i == i && sub_j == j)      continue;

            if (!is_wall(grid, sub_i, sub_j))
            {
                new_edges[n_edges].id = mat2list(sub_i, sub_j, CELLX);
                new_edges[n_edges++].weight = 1;
            }

        }

        new_vert = malloc(sizeof(A_Vertex));
        new_vert -> id        = mat2list(i, j, CELLX );
        new_vert -> data      = &(grid -> cells[i][j] -> clr);
        new_vert -> heuristic = 1.5 * ( abs(d_pos[1] - i) + abs(d_pos[0]-j) );
        new_vert -> edges     = new_edges;
        new_vert -> n_edges   = n_edges;

        verts[new_vert -> id] = new_vert;
    }

    clock_t start, end;
    double cpu_time_used;

    unsigned start_id = mat2list(s_pos[1], s_pos[0], CELLX);
    unsigned end_id   = mat2list(d_pos[1], d_pos[0], CELLX);

    start = clock();
    LinkedList *path = a_star(verts, n_verts, start_id, end_id, main_render, grid);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("TIME: %lf\n", cpu_time_used);

    if (path == NULL) {
        printf("No Path Found\n");
        grid -> cells[s_pos[1]][s_pos[0]] -> clr = START_CLR;
        restore_grid(grid);

    } else {
        int x, y;
        int prev_x = s_pos[0], prev_y = s_pos[1];

        struct _ll_node *curr_node = path -> head;

        while (curr_node != NULL) {
            x = ((A_Vertex*) curr_node -> payload) -> id % grid -> CELLX;
            y = ((A_Vertex*) curr_node -> payload) -> id / grid -> CELLX;

            grid -> cells[prev_y][prev_x] -> clr = PATH_CLR;
            grid -> cells[y][x] -> clr = START_CLR;

            grid_draw(main_render, grid);
            SDL_Delay(25);

            prev_x = x;
            prev_y = y;
            curr_node = curr_node -> next;
        }

        // Free Memory
        for (int i = 0; i < n_verts; i++)
        {
            if (verts[i] == NULL) continue;
            free(verts[i] -> edges);
            free(verts[i]);
        }

        LL_destroy(path);
    }
}

int is_wall (GRID *grid, int i, int j) {
    COLOR curr = grid -> cells[i][j] -> clr;
    return curr.r == WALL_CLR.r &&
           curr.g == WALL_CLR.g &&
           curr.b == WALL_CLR.b;
}

void restore_grid ( GRID *grid ) {
    for (int i = 0; i < grid -> CELLY; i++)
    for (int j = 0; j < grid -> CELLX; j++)
    {
        if (is_wall(grid, i, j)) continue;
        if (i == s_pos[1] && j == s_pos[0])
            grid -> cells[i][j] -> clr = START_CLR; 

        else if (i == d_pos[1] && j == d_pos[0])
            grid -> cells[i][j] -> clr = DEST_CLR; 

        else
            grid -> cells[i][j] -> clr = VOID_CLR; 
    }
}

unsigned mat2list( unsigned i, unsigned j, unsigned width ) {
    return i * width + j; 
}

