#include "api.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <queue>
#include <set>
#include <spdlog/spdlog.h>

using namespace cycles;

// Custom exception for critical bot failures :(
class BotException : public std::exception {
    std::string message;

public:
    explicit BotException(const std::string &msg) : message(msg) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

// Defining a custom comparator for sf::Vector2i
// given that it is not directly comparable 
struct Vector2Comparator {
    bool operator()(const sf::Vector2i &a, const sf::Vector2i &b) const {
        return (a.x < b.x) || (a.x == b.x && a.y < b.y); // 
    }
};

// Terminator bot ta=hat targets the nearest opponent until terminated >:)
// Only falls back to random moves when in a tight spot
// Uses a combination of safety, proximity, trapping potential, and available space
// to calculate the best move

class AggressiveTargetBot {
    Connection connection;
    std::string name;
    GameState state;
    Player myPlayer;

    // Computes Manhattan distance between two positions
    int calculateDistance(sf::Vector2i a, sf::Vector2i b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    // Find the closest opponent's position using Manhattan distance
    Player* findNearestOpponent() {
        sf::Vector2i myPos = myPlayer.position;
        Player* nearestOpponent = nullptr;
        int minDistance = std::numeric_limits<int>::max();

        for (auto &player : state.players) {
            if (player.id != myPlayer.id) { // Skip self
                int distance = calculateDistance(myPos, player.position);
                if (distance < minDistance) {
                    minDistance = distance;
                    nearestOpponent = &player;
                }
            }
        }
        return nearestOpponent;
    }

    // Predict the opponent's next move based on their position
    sf::Vector2i predictOpponentMove(Player* opponent) {
        sf::Vector2i oppPos = opponent->position;

        for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i newPos = oppPos + getDirectionVector(direction);
            if (state.isInsideGrid(newPos) && state.isCellEmpty(newPos)) {
                return newPos; // Return the first valid move
            }
        }
        return oppPos; // No valid moves, opponent might stay in place
    }

    // Calculate available space from a position
    int calculateAvailableSpace(sf::Vector2i pos) {
        int space = 0;
        std::queue<sf::Vector2i> toVisit;
        std::set<sf::Vector2i, Vector2Comparator> visited; // Use custom comparator to avoid errors

        toVisit.push(pos);
        visited.insert(pos);

        while (!toVisit.empty() && space < 20) { // Limit to avoid over-expanding
            sf::Vector2i current = toVisit.front();
            toVisit.pop();
            space++;

            for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
                sf::Vector2i neighbor = current + getDirectionVector(direction);
                if (state.isInsideGrid(neighbor) && state.isCellEmpty(neighbor) &&
                    visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    toVisit.push(neighbor);
                }
            }
        }

        return space; // Returns a measure of how "open" the area is
    }

    // Check if the bot is in a tight spot
    bool isInTightSpot() {
        int space = calculateAvailableSpace(myPlayer.position);
        return space < 5; // Threshold for when the bot is in a tight spot
    }

    // Rank potential moves with additional survival considerations
    Direction decideBestMove(sf::Vector2i target, sf::Vector2i predictedOpponentPos) {
        std::vector<std::pair<Direction, int>> rankedMoves;
        sf::Vector2i myPos = myPlayer.position;

        for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i newPos = myPos + getDirectionVector(direction);

            if (state.isInsideGrid(newPos) && state.isCellEmpty(newPos)) {
                int safetyScore = calculateSafety(newPos);
                int proximityScore = -calculateDistance(newPos, target); // Negative for closer proximity
                int trappingScore = calculateTrappingPotential(newPos, predictedOpponentPos);
                int spaceScore = calculateAvailableSpace(newPos); // Higher for more open space

                // Combine scores
                int totalScore = safetyScore + proximityScore + trappingScore + spaceScore;
                rankedMoves.emplace_back(direction, totalScore);
            }
        }

        // Sort moves by combined score
        std::sort(rankedMoves.begin(), rankedMoves.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });

        // Return best-ranked move or fallback
        if (!rankedMoves.empty()) {
            return rankedMoves.front().first;
        }

        return randomDirection(); // Fallback if no good moves are found
    }

    // Calculate a safety score for a position
    int calculateSafety(sf::Vector2i pos) {
        int safetyScore = 0;

        for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i neighbor = pos + getDirectionVector(direction);
            if (!state.isInsideGrid(neighbor) || !state.isCellEmpty(neighbor)) {
                safetyScore -= 10; // Higher penalty for closer obstacles
            }
        }
        return safetyScore;
    }

    // Calculate the trapping potential for a position
    int calculateTrappingPotential(sf::Vector2i pos, sf::Vector2i predictedOpponentPos) {
        int trappingPotential = 0;

        for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
            sf::Vector2i newPos = predictedOpponentPos + getDirectionVector(direction);
            if (!state.isInsideGrid(newPos) || !state.isCellEmpty(newPos)) {
                trappingPotential += 5; // Reward reducing opponent's escape routes
            }
        }
        return trappingPotential;
    }

    // Picks a random valid direction
    Direction randomDirection() {
        std::vector<Direction> directions = {Direction::north, Direction::east, Direction::south, Direction::west};
        for (Direction direction : directions) {
            sf::Vector2i newPos = myPlayer.position + getDirectionVector(direction);
            if (state.isInsideGrid(newPos) && state.isCellEmpty(newPos)) {
                return direction;
            }
        }
        return Direction::north; // Default fallback
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
        Player* nearestOpponent = findNearestOpponent();
        if (nearestOpponent == nullptr) {
            spdlog::info("{}: No targets remaining, stopping.", name);
            exit(0); // Exit when no targets remain
        }

        if (isInTightSpot()) {
            spdlog::warn("{}: Activating escape mode!", name);
            Direction move = randomDirection(); // Escape mode prioritizes random valid move
            connection.sendMove(move);
        } else {
            sf::Vector2i predictedOpponentPos = predictOpponentMove(nearestOpponent);
            Direction move = decideBestMove(nearestOpponent->position, predictedOpponentPos);
            connection.sendMove(move);
        }
    }

public:
    AggressiveTargetBot(const std::string &botName) : name(botName) {
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
    AggressiveTargetBot bot(botName);
    bot.run();
    return 0;
}
