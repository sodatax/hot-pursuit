#include <bn_core.h>
#include <bn_display.h>
#include <bn_keypad.h>
#include <bn_random.h>
#include <bn_rect.h>
#include <bn_size.h>
#include <bn_string.h>
#include <bn_sprite_ptr.h>
#include <bn_sprite_text_generator.h>
#include <bn_log.h>

#include "common_fixed_8x16_font.h"
#include "bn_sprite_items_dot.h"
#include "bn_sprite_items_square.h"

// Width and height of the the player bounding box
static constexpr bn::size PLAYER_SIZE = {8, 8};
static constexpr bn::size ENEMY_SIZE = {8, 8};

static constexpr int MIN_Y = -bn::display::height() / 2;
static constexpr int MAX_Y = bn::display::height() / 2;
static constexpr int MIN_X = -bn::display::width() / 2;
static constexpr int MAX_X = bn::display::width() / 2;

// Number of characters required to show two of the longest numer possible in an int (-2147483647)
static constexpr int MAX_SCORE_CHARS = 22;

// Score location
static constexpr int SCORE_X = 70;
static constexpr int SCORE_Y = -70;

// High score location
static constexpr int HIGH_SCORE_X = -70;
static constexpr int HIGH_SCORE_Y = -70;

/**
 * Creates a rectangle centered at a sprite's location with a given size.
 * sprite the sprite to center the box around
 * box_size the dimensions of the bounding box
 */
bn::rect create_bounding_box(bn::sprite_ptr sprite, bn::size box_size)
{
    return bn::rect(sprite.x().round_integer(),
                    sprite.y().round_integer(),
                    box_size.width(),
                    box_size.height());
}

/**
 * Displays a score and high score.
 *
 * Score starts a 0 and is increased each time update is called, and reset to 0 when resetScore is
 * called. High score tracks the highest score ever reached.
 */
class ScoreDisplay
{
public:
    ScoreDisplay() : score(0),                                                                 // Start score at 0
                     high_score(0),                                                            // Start high score at 0
                     score_sprites(bn::vector<bn::sprite_ptr, MAX_SCORE_CHARS>()),             // Start with empty vector for score sprites
                     text_generator(bn::sprite_text_generator(common::fixed_8x16_sprite_font)) // Use a new text generator
    {
    }

    /**
     * Increases score by 1 and updates high score if needed. Displays score and high score.
     */
    void update()
    {
        // increase score and update high score if this is the new highest
        score++;
        if (score > high_score)
        {
            high_score = score;
        }

        // Stop displaying previous scores
        score_sprites.clear();

        // Display new scores
        show_number(SCORE_X, SCORE_Y, score);
        show_number(HIGH_SCORE_X, HIGH_SCORE_Y, high_score);
    }

    /**
     * Displays a number at the given position
     */
    void show_number(int x, int y, int number)
    {
        // Convert number to a string and then display it
        bn::string<MAX_SCORE_CHARS> number_string = bn::to_string<MAX_SCORE_CHARS>(number);
        text_generator.generate(x, y, number_string, score_sprites);
    }

    /**
     * Sets score back to 0. Does NOT reset high score.
     */
    void resetScore()
    {
        score = 0;
    }

    int score;                                                 // current score
    int high_score;                                            // best core
    bn::vector<bn::sprite_ptr, MAX_SCORE_CHARS> score_sprites; // Sprites to display scores
    bn::sprite_text_generator text_generator;                  // Text generator for scores
};

class Player
{
public:
    Player(int starting_x, int starting_y, bn::fixed player_speed, bn::size player_size) : sprite(bn::sprite_items::dot.create_sprite(starting_x, starting_y)),
                                                                                           speed(player_speed),
                                                                                           size(player_size),
                                                                                           bounding_box(create_bounding_box(sprite, size))
    {
    }

    /**
     * Update the position and bounding box of the player based on d-pad movement.
     */
    void update()
    {
        if (bn::keypad::right_held() && !(sprite.x() >= MAX_X - 4))
        {
            sprite.set_x(sprite.x() + speed);
        }
        if (bn::keypad::left_held() && !(sprite.x() <= MIN_X + 4))
        {
            sprite.set_x(sprite.x() - speed);
        }
        if (bn::keypad::up_held() && !(sprite.y() <= MIN_Y + 4))
        {
            sprite.set_y(sprite.y() - speed);
        }
        if (bn::keypad::down_held() && !(sprite.y() >= MAX_Y - 4))
        {
            sprite.set_y(sprite.y() + speed);
        }
        bounding_box = create_bounding_box(sprite, size);
    }

    // Create the sprite. This will be moved to a constructor
    bn::sprite_ptr sprite;
    bn::fixed speed;       // The speed of the player
    bn::size size;         // The width and height of the sprite
    bn::rect bounding_box; // The rectangle around the sprite for checking collision
};

class Enemy
{
public:
    Enemy(int starting_x, int starting_y, bn::fixed enemy_speed, bn::size enemy_size) : enemy_sprite(bn::sprite_items::square.create_sprite(starting_x, starting_y)),
                                                                                        speed(enemy_speed),
                                                                                        size(enemy_size),
                                                                                        bounding_box(create_bounding_box(enemy_sprite, size))
    {
    }

    /**
     * Update the position and bounding box of the player based on d-pad movement.
     */
    void update(Player player)
    {
        bn::fixed player_X = player.sprite.x();
        bn::fixed player_Y = player.sprite.y();

        if (enemy_sprite.x() > player_X)
        {
            enemy_sprite.set_x(enemy_sprite.x() - speed);
        }
        if (enemy_sprite.x() < player_X)
        {
            enemy_sprite.set_x(enemy_sprite.x() + speed);
        }
        if (enemy_sprite.y() > player_Y)
        {
            enemy_sprite.set_y(enemy_sprite.y() - speed);
        }
        if (enemy_sprite.y() < player_Y)
        {
            enemy_sprite.set_y(enemy_sprite.y() + speed);
        }

        bounding_box = create_bounding_box(enemy_sprite, size);
    }

    // Create the sprite. This will be moved to a constructor
    bn::sprite_ptr enemy_sprite;
    bn::fixed speed;       // The speed of the player
    bn::size size;         // The width and height of the sprite
    bn::rect bounding_box; // The rectangle around the sprite for checking collision
};

void levelUpdate(ScoreDisplay &scoreDisplay, bn::vector<Enemy, 20> &enemys, bn::random &rng)
{
    int spots = 19;
    if (scoreDisplay.score % 300 == 0 && scoreDisplay.score != 0)
    {
        spots--;

        int new_x = rng.get_int(MIN_X, MAX_X);
        int new_y = rng.get_int(MIN_Y, MAX_Y);

        enemys.push_back(Enemy(new_x, new_y, 2, ENEMY_SIZE));
    }
}

int main()
{
    bn::core::init();

    bn::vector<Enemy, 20> enemys = {};

    bn::random rng = bn::random();

    // Create a new score display
    ScoreDisplay scoreDisplay = ScoreDisplay();

    Player player = Player(35, 22, 7, PLAYER_SIZE);
    enemys.push_back(Enemy(30, 50, 2, ENEMY_SIZE));

    while (true)
    {
        player.update();
        for (Enemy &enemy : enemys)
        {
            enemy.update(player);
            if (enemy.bounding_box.intersects(player.bounding_box))
            {
                scoreDisplay.resetScore();
                player.sprite.set_x(44);
                player.sprite.set_y(22);

                int new_x = rng.get_int(MIN_X, MAX_X);
                int new_y = rng.get_int(MIN_Y, MAX_Y);

                enemys.clear();

                enemys.push_back(Enemy(new_x, new_y, 2, ENEMY_SIZE));
                break;
            }
        }
        levelUpdate(scoreDisplay, enemys, rng);

        // Update the scores and disaply them
        scoreDisplay.update();

        bn::core::update();
    }
}