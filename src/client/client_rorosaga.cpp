#include "api.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

using namespace cycles;

class ZigzagBot {
    Connection connection;
    std::string name;
    GameState state;
    Player myPlayer;
    bool movingDown = true; // Track whether the bot is zigzagging down or up
    Direction primaryDirection = Direction::east; // Main progression direction

    // Check if a move is valid
    bool isValidMove(Direction direction) {
        sf::Vector2i newPos = myPlayer.position + getDirectionVector(direction);
        return state.isInsideGrid(newPos) && state.isCellEmpty(newPos);
    }

    // Get the next move for zigzagging
    Direction decideMove() {
        Direction zigzagDirection = movingDown ? Direction::south : Direction::north;

        // Prioritize zigzag movement (up or down), but fall back to the primary direction
        if (isValidMove(zigzagDirection)) {
            return zigzagDirection;
        }

        // If zigzag direction is blocked, try the primary direction (e.g., moving east)
        if (isValidMove(primaryDirection)) {
            // Reverse zigzag direction when moving to the next column
            movingDown = !movingDown;
            return primaryDirection;
        }

        // If both zigzag and primary directions are blocked, fallback to staying still
        return primaryDirection; // Avoid breaking the pattern
    }

    // Update the bot's state with the current game state
    void updateState() {
        state = connection.receiveGameState();
        for (const auto &player : state.players) {
            if (player.name == name) {
                myPlayer = player;
                break;
            }
        }
    }

    // Send the decided move to the server
    void sendMove() {
        Direction move = decideMove();
        connection.sendMove(move);
    }

public:
    ZigzagBot(const std::string &botName) : name(botName) {
        // Connect to the server
        connection.connect(name);
        if (!connection.isActive()) {
            spdlog::critical("{}: Connection failed", name);
            exit(1);
        }
    }

    // Run the bot in a loop
    void run() {
        while (connection.isActive()) {
            updateState();
            sendMove();
        }
    }
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
        return 1;
    }

    std::string botName = argv[1];
    ZigzagBot bot(botName);
    bot.run();
    return 0;
}
