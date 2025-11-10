#include "eval.hpp"

// ==================== INITIALIZATION ====================

BoardEvaluator::BoardEvaluator(MoveGenerator* moveGen)
    : moveGenerator(moveGen),
      defense_mode(false),
      river_flows_computed(false),
      scoring_areas_initialized(false),
      cached_board_hash(0),
      weight1(50000.0f),
      weight2(-5.0f),
      weight_river_mobility(0.15f),
      weight_river_combos(0.05f),
      weight_board_adv(0.15f),
      weight_defense(DEFENSE_BOT_DEFENSE) {}

void BoardEvaluator::setMoveGenerator(MoveGenerator* moveGen) {
    moveGenerator = moveGen;
}

void BoardEvaluator::setDefenseMode(bool mode) {
    defense_mode = mode;
    weight_defense = mode ? DEFENSE_BOT_DEFENSE : OFFENSE_BOT_DEFENSE;
}

void BoardEvaluator::initializeScoringAreas(const GameState& gameState) const {
    if (scoring_areas_initialized) return;

    int rows = gameState.getRows();
    const std::vector<int>& score_cols = gameState.getScoreCols();

    circle_scoring.row = 2;
    circle_scoring.score_cols = score_cols;

    square_scoring.row = rows - 3;
    square_scoring.score_cols = score_cols;

    scoring_areas_initialized = true;
}

// ==================== EVALUATION ENTRY ====================

float BoardEvaluator::EvaluateBoard(const GameState& gameState, bool isCirclePlayer) const {
    initializeScoringAreas(gameState);

    float score1 = computeBasicEvaluation(gameState, isCirclePlayer);
    float score2 = evaluatePosition(gameState, isCirclePlayer);
    float score4 = evaluateMobility(gameState, isCirclePlayer);

    std::vector<RiverSimulationCacheEntry> simulation_cache;
    simulation_cache.reserve(20);

    float river_mobility = evaluateRiverMobility(gameState, isCirclePlayer, simulation_cache);
    float river_combos = evaluateRiverCombos(gameState, isCirclePlayer, simulation_cache);

    float base_score = score1 * weight1 + score2 * weight2;
    float river_score_tot = river_mobility * weight_river_mobility + river_combos * weight_river_combos;

    float board_adv = evaluateBoardAdvancement(gameState, isCirclePlayer);
    float board_adv_score = board_adv * weight_board_adv;

    float defense_score = evaluateDefense(gameState, isCirclePlayer);

    return base_score + river_score_tot + board_adv_score + defense_score * weight_defense;
}

// ==================== BASIC + POSITIONAL ====================

float BoardEvaluator::computeBasicEvaluation(const GameState& gameState, bool isCirclePlayer) const {
    float score = 0.0f;
    int player_stones = countStonesInScoringArea(gameState, isCirclePlayer);
    int opp_stones = countStonesInScoringArea(gameState, !isCirclePlayer);
    score += player_stones * 2.0f;
    score -= opp_stones * 2.0f;

    int player_rivers = countRiversInScoringArea(gameState, isCirclePlayer);
    int opp_rivers = countRiversInScoringArea(gameState, !isCirclePlayer);
    score += player_rivers * 1.0f;
    score -= opp_rivers * 1.0f;
    return score;
}

float BoardEvaluator::evaluatePosition(const GameState& gameState, bool isCirclePlayer) const {
    std::vector<int> player_distances = getmoveDistancesFromScoringArea(gameState, isCirclePlayer);
    float sum = 0.0f;
    for (int i = 0; i < 4 && i < (int)player_distances.size(); ++i) {
        sum += player_distances[i];
    }
    return sum;
}

float BoardEvaluator::evaluateMobility(const GameState& gameState, bool isCirclePlayer) const {
    int my_mobile_stones = countStonesAdjacentToRivers(gameState, isCirclePlayer);
    (void)countStonesAdjacentToRivers(gameState, !isCirclePlayer);
    return (float)my_mobile_stones;
}

// ==================== DEFENSE + ADVANCEMENT + RIVERS ====================

// (Full definitions identical to your provided code)
// Copy the remaining private method bodies verbatim here.
// No logic or numerical changes, only moved from header.


float BoardEvaluator::evaluateDefense(const GameState& gameState, bool isCirclePlayer) const {
    float defense_score = 0.0f;
    
    // Get opponent's scoring area info
    const auto& score_cols = gameState.getScoreCols();
    int opponent_scoring_row = isCirclePlayer ? square_scoring.row : circle_scoring.row; // Opponent's scoring row
    
    // Evaluate defense around each scoring column (4 positions total)
    for (int score_col : score_cols) {
        defense_score += evaluateColumnDefense(gameState, score_col, opponent_scoring_row, isCirclePlayer);
    }
    
    return defense_score;
}

// Evaluate defense around a single scoring column
float BoardEvaluator::evaluateColumnDefense(const GameState& gameState, int score_col, int opponent_scoring_row, bool isCirclePlayer) const {
    float column_defense = 0.0f;
    
    // Defense positions around the scoring area:
    // - 4 positions above scoring row
    // - 4 positions below scoring row  
    // - 1 position on each side (same row as scoring row)
    
    int defense_row = opponent_scoring_row - 1;
    column_defense += evaluateDefensePosition(gameState, score_col, defense_row, isCirclePlayer, "vertical");
    
    defense_row = opponent_scoring_row + 1;
    column_defense += evaluateDefensePosition(gameState, score_col, defense_row, isCirclePlayer, "vertical");
    
    
    // 3. Check sideways positions (same row as scoring row)
    // Left side
    if (score_col==gameState.getScoreCols().front()) {
        column_defense += evaluateDefensePosition(gameState, score_col - 1 , opponent_scoring_row, isCirclePlayer, "horizontal");
    }

    // Right side
    if (score_col==gameState.getScoreCols().back()) {
        column_defense += evaluateDefensePosition(gameState, score_col + 1, opponent_scoring_row, isCirclePlayer, "horizontal");
    }
    
    return column_defense;
}

// Evaluate a single defense position
float BoardEvaluator::evaluateDefensePosition(const GameState& gameState, int x, int y, bool isCirclePlayer, const std::string& position_type) const {
    uint8_t piece = gameState.getPiece(x, y);
    
    if (piece == EMPTY) {
        return 0.0f; // No defense value from empty positions
    }
    
    // Check if this is our piece (defending) or opponent's piece
    bool isOurPiece = isCirclePlayer ? ::isCircle(piece) : ::isSquare(piece);
    
    if (!isOurPiece) {
        return -60.0f; // Opponent's pieces don't contribute to our defense
    }
    
    // Our piece is in a defense position - evaluate based on piece type and position type
    
    if (::isStone(piece)) {
        // Stones in defense positions get positive reward
        if (position_type == "vertical") {
            return 5.0f; // Stones above/below scoring area
        } else { // horizontal (sideways)
            return 12.0f; // Stones on sides of scoring area (more valuable)
        }
    }
    
    if (::isRiver(piece)) {
        // Rivers get different rewards based on orientation and position
        bool isHorizontalRiver = ::isHorizontal(piece);
        
        if (position_type == "vertical") {
            if (isHorizontalRiver) {
                // Vertical rivers on sideways positions - HIGH POSITIVE REWARD
                return 5.0f; // Very good - vertical rivers block horizontal access
            } else {
                // Horizontal rivers on sideways positions - HIGHLY NEGATIVE REWARD
                return -5.0f; // Very bad - horizontal rivers create pathways for opponent
            }
        } else { // horizontal (sideways positions)
            if (!isHorizontalRiver) {
                // Vertical rivers on sideways positions - HIGH POSITIVE REWARD
                return 15.0f; // Very good - vertical rivers block horizontal access
            } else {
                // Horizontal rivers on sideways positions - HIGHLY NEGATIVE REWARD
                return -30.0f; // Very bad - horizontal rivers create pathways for opponent
            }
        }
    }
    
    return 0.0f; // Default case
}

// ENHANCED: Ring-based advancement evaluation with Manhattan distance zones
float BoardEvaluator::evaluateBoardAdvancement(const GameState& gameState, bool isCirclePlayer) const {
    float advancement_score = 0.0f;
    const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
    const auto& score_cols = gameState.getScoreCols();
    
    int rows = gameState.getRows();
    int target_scoring_row = isCirclePlayer ? 2 : (rows - 3);  // Circle scores at row 2, Square at row (rows-3)
    
    // Count pieces in each Manhattan distance ring
    int pieces_in_ring[5] = {0, 0, 0, 0, 0};  // rings 0, 1, 2, 3, 4+
    int stones_in_ring[5] = {0, 0, 0, 0, 0};  // stones specifically in each ring
    
    for (const auto& pos : player_positions) {
        int x = pos.first, y = pos.second;
        uint8_t piece = gameState.getPiece(x, y);
        
        // Calculate minimum Manhattan distance to any scoring position
        int min_manhattan_distance = 100;
        for (int score_col : score_cols) {
            int manhattan_dist = abs(x - score_col) + abs(y - target_scoring_row);
            min_manhattan_distance = std::min(min_manhattan_distance, manhattan_dist);
        }
        
        
        if (min_manhattan_distance > 4) continue; // Ignore pieces too far away
        // Clamp distance to ring index (max ring 4 for distance 4+)
        int ring = min_manhattan_distance;
        
        pieces_in_ring[ring]++;
        
        // Track stones separately for extra scoring potential
        if (::isStone(piece)) {
            stones_in_ring[ring]++;
        }
    }
    
    // RING-BASED SCORING: Closer rings get exponentially higher rewards
    
    // Already accounted for in basic evaluation
    // // Ring 0 (distance 0): Already in scoring area - HUGE bonus
    // advancement_score += pieces_in_ring[0] * 50.0f;  // +50 per piece in scoring area
    // advancement_score += stones_in_ring[0] * 100.0f; // +100 extra for stones (they can score!)
    
    // Ring 1 (distance 1): Adjacent to scoring - VERY HIGH bonus  
    advancement_score += pieces_in_ring[1] * 60.0f;  // +60 per piece 1 move from scoring
    advancement_score += stones_in_ring[1] * 120.0f;  // +120 extra for stones (high scoring potential)
    
    // Ring 2 (distance 2): Close to scoring - HIGH bonus
    advancement_score += pieces_in_ring[2] * 8.0f;   // +8 per piece 2 moves from scoring  
    advancement_score += stones_in_ring[2] * 15.0f;  // +15 extra for stones (good scoring potential)
    
    // Ring 3 (distance 3): Moderate distance - MEDIUM bonus
    advancement_score += pieces_in_ring[3] * 3.0f;   // +3 per piece 3 moves from scoring
    advancement_score += stones_in_ring[3] * 5.0f;   // +5 extra for stones (some scoring potential)
    
    // Ring 4+ (distance 4+): Far from scoring - SMALL bonus
    advancement_score += pieces_in_ring[4] * 1.0f;   // +1 per piece 4+ moves from scoring
    advancement_score += stones_in_ring[4] * 1.0f;   // +1 extra for stones (minimal scoring potential)
    
    // BONUS: Reward pieces in scoring columns (even if not at scoring row)
    float column_bonus = 0.0f;
    for (const auto& pos : player_positions) {
        int x = pos.first, y = pos.second;
        
        // Check if piece is in any scoring column
        for (int score_col : score_cols) {
            if (x == score_col) {
                uint8_t piece = gameState.getPiece(x, y);
                
                // Distance to scoring row (vertical distance only)
                int vertical_distance = abs(y - target_scoring_row);
                
                if (vertical_distance == 1) {
                    // One row away from scoring
                    column_bonus += ::isStone(piece) ? 15.0f : 6.0f;
                } else if (vertical_distance == 2) {
                    // Two rows away from scoring
                    column_bonus += ::isStone(piece) ? 8.0f : 3.0f;
                } else if (vertical_distance <= 4) {
                    // 3-4 rows away from scoring
                    column_bonus += ::isStone(piece) ? 3.0f : 1.0f;
                }
                break; // Only count each piece once
            }
        }
    }
    
    advancement_score += column_bonus;
    
    return advancement_score;
}

// ================= RIVER-BUILDING EVALUATION ========================
    
    // Evaluate river combinations - rivers that connect to extend kreach
float BoardEvaluator::evaluateRiverCombos(const GameState& gameState, bool isCirclePlayer, std::vector<RiverSimulationCacheEntry>& cache) const {
    float combo_score = 0.0f;
    const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
    
    // Find all our rivers
    std::vector<std::pair<int,int>> our_rivers;
    for (const auto& pos : player_positions) {
        uint8_t piece = gameState.getPiece(pos.first, pos.second);
        if (::isRiver(piece)) {
            our_rivers.push_back(pos);
        }
    }
    
    // Check each river pair for connectivity
    for (size_t i = 0; i < our_rivers.size(); ++i) {
        for (size_t j = i + 1; j < our_rivers.size(); ++j) {
            float connection_score = evaluateRiverConnectionWithCache(
            gameState, our_rivers[i], our_rivers[j], isCirclePlayer, cache);
            combo_score += connection_score;
        }
    }
    
    return combo_score;
}

// Get cached river simulation using simple vector lookup
std::vector<std::pair<int,int>> BoardEvaluator::getCachedRiverSimulation(
    const GameState& gameState, int river_x, int river_y, bool isCirclePlayer,
    std::vector<RiverSimulationCacheEntry>& cache) const {
    
    // STEP 1: Check cache for existing entry (O(n) lookup where n = cache size)
    for (const auto& entry : cache) {
        if (entry.river_x == river_x && entry.river_y == river_y &&
            entry.isCirclePlayer == isCirclePlayer) {
            
            // CACHE HIT!
            return entry.simulation_result;
        }
    }
    
    // STEP 2: Cache miss - compute using existing method
    auto simulation_result = simulateRiverFlow(gameState, river_x, river_y, isCirclePlayer);
    
    // STEP 3: Store result in cache for future lookups
    cache.emplace_back(river_x, river_y, isCirclePlayer, simulation_result);
    
    return simulation_result;
}

// Change return type from int to float
// UPDATED: countReachableOpponentPositions with cache parameter
float BoardEvaluator::countReachableOpponentPositionsWithCache(const GameState& gameState, int river_x, int river_y, 
                                            bool isCirclePlayer,
                                            std::vector<RiverSimulationCacheEntry>& cache) const {
    float weighted_score = 0.0f;
    int opponent_half_start = isCirclePlayer ? 0 : (gameState.getRows() / 2);
    int opponent_half_end = isCirclePlayer ? (gameState.getRows() / 2) : gameState.getRows();
    
    // Get our goal zone info for distance calculation
    const auto& score_cols = gameState.getScoreCols();
    int our_goal_row = isCirclePlayer ? 2 : (gameState.getRows() - 3);
    
    // OPTIMIZED: Use cached simulation instead of direct call
    std::vector<std::pair<int,int>> destinations = getCachedRiverSimulation(
        gameState, river_x, river_y, isCirclePlayer, cache);
    
    for (const auto& dest : destinations) {
        int dest_x = dest.first, dest_y = dest.second;
        // Check if destination is in opponent territory
        if (dest_y >= opponent_half_start && dest_y < opponent_half_end) {
            
            // Calculate distance to nearest goal zone position
            int min_distance = 100;
            for (int goal_col : score_cols) {
                int distance = abs(dest_x - goal_col) + abs(dest_y - our_goal_row);
                min_distance = std::min(min_distance, distance);
            }
            
            // Weight: closer to goal = higher score
            float weight = std::max(1.0f, 4.0f - min_distance);
            weighted_score += weight;
        }
    }
    
    return weighted_score;
}


// UPDATED: countReachableScoringPositions with cache parameter
float BoardEvaluator::countReachableScoringPositionsWithCache(const GameState& gameState, int river_x, int river_y, 
                                            bool isCirclePlayer,
                                            std::vector<RiverSimulationCacheEntry>& cache) const { 
    float weighted_score = 0.0f;  // Changed from int scoring_reach = 0
    const auto& score_cols = gameState.getScoreCols();
    int target_scoring_row = isCirclePlayer ? 2 : (gameState.getRows() - 3);
    
    // OPTIMIZED: Use cached simulation instead of direct call
    std::vector<std::pair<int,int>> destinations = getCachedRiverSimulation(
        gameState, river_x, river_y, isCirclePlayer, cache);
    
    for (const auto& dest : destinations) {
        int dest_x = dest.first, dest_y = dest.second;
        
        // Calculate distance to nearest scoring column
        int min_distance_to_scoring = 100;
        for (int score_col : score_cols) {
            int distance = abs(dest_x - score_col) + abs(dest_y - target_scoring_row);
            min_distance_to_scoring = std::min(min_distance_to_scoring, distance);
        }
        
        // Weight positions closer to scoring area more heavily
        if (min_distance_to_scoring <= 3) {  // Only count positions reasonably close to scoring
            float weight = std::max(1.0f, 4.0f - min_distance_to_scoring);
            weighted_score += weight;
            
            // Extra bonus if this position is exactly in scoring area
            if (dest_y == target_scoring_row) {
                for (int score_col : score_cols) {
                    if (dest_x == score_col) {
                        weighted_score += 2.0f;  // Big bonus for direct scoring positions
                        break;
                    }
                }
            }
        }
    }
    
    return weighted_score;  // Changed from return scoring_reach
}

// Simple river flow simulation - just follow the river orientation
std::vector<std::pair<int,int>> BoardEvaluator::simulateRiverFlow(const GameState& gameState, int start_x, int start_y, bool isCirclePlayer) const {
    std::vector<std::pair<int,int>> reachable_positions;
    uint8_t river_piece = gameState.getPiece(start_x, start_y);
    
    if (!::isRiver(river_piece)) return reachable_positions;
    
    // Determine flow directions based on river orientation
    std::vector<std::pair<int,int>> directions;
    if (::isHorizontal(river_piece)) {
        directions = {{1, 0}, {-1, 0}}; // left and right
    } else {
        directions = {{0, 1}, {0, -1}}; // up and down
    }
    
    // Follow each direction until blocked
    for (const auto& dir : directions) {
        int current_x = start_x + dir.first;
        int current_y = start_y + dir.second;
        
        // Follow this direction while we can
        while (gameState.inBounds(current_x, current_y)) {
            uint8_t current_piece = gameState.getPiece(current_x, current_y);
            
            if (current_piece == EMPTY) {
                // Empty position - can reach here
                reachable_positions.push_back({current_x, current_y});
                current_x += dir.first;
                current_y += dir.second;
            } else if (::isRiver(current_piece)) {
                // Another river - continue flow but don't count as reachable (already has piece)
                current_x += dir.first;
                current_y += dir.second;
            } else {
                // Stone or opponent piece - flow stops
                break;
            }
        }
    }
    
    return reachable_positions;
}


// Evaluate how well two rivers connect/combo together
float BoardEvaluator::evaluateRiverConnectionWithCache(const GameState& gameState, const std::pair<int,int>& river1, 
                            const std::pair<int,int>& river2, bool isCirclePlayer, std::vector<RiverSimulationCacheEntry>& cache) const {
    float connection_score = 0.0f;
    
    // Get flow destinations from both rivers
    auto destinations1 = getCachedRiverSimulation(gameState, river1.first, river1.second, isCirclePlayer, cache);
    auto destinations2 = getCachedRiverSimulation(gameState, river2.first, river2.second, isCirclePlayer, cache);
    
    // Check if river1 can reach river2's position (or vice versa)
    for (const auto& dest : destinations1) {
        if (dest == river2) {
            connection_score += 1.0f; // Direct connection bonus
            break;
        }
    }
    
    // Check for overlapping reachable areas (rivers that reach the same strategic areas)
    int shared_territory = 0;
    for (const auto& dest1 : destinations1) {
        for (const auto& dest2 : destinations2) {
            // If both rivers can reach positions close to each other
            int distance = abs(dest1.first - dest2.first) + abs(dest1.second - dest2.second);
            if (distance <= 2) { // Within 2 Manhattan distance
                shared_territory++;
            }
        }
    }
    
    connection_score += shared_territory * 0.2f; // Bonus for shared territorial control
    
    return connection_score;
}

// Simple mobility evaluation - count reachable positions in opponent area
float BoardEvaluator::evaluateRiverMobility(const GameState& gameState, bool isCirclePlayer, 
    std::vector<RiverSimulationCacheEntry>& cache) const {

    float mobility_score = 0.0f;
    const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);


    // Check each of our rivers
    for (const auto& pos : player_positions) {
        int x = pos.first, y = pos.second;
        uint8_t piece = gameState.getPiece(x, y);
        
        if (::isRiver(piece)) {
            // OPTIMIZED: Use cached simulation for both calls
            float reachable_opponent_positions = countReachableOpponentPositionsWithCache(
                gameState, x, y, isCirclePlayer, cache);
            mobility_score += reachable_opponent_positions * 0.15f;
            
            // Bonus for reaching scoring area specifically
            float scoring_area_reach = countReachableScoringPositionsWithCache(
                gameState, x, y, isCirclePlayer, cache);
            mobility_score += scoring_area_reach * 0.85f;
        }
    }
    
    return mobility_score;
}

// Count stones in scoring area - helper function
int BoardEvaluator::countStonesInScoringArea(const GameState& gameState, bool isCirclePlayer) const {
    int count = 0;
    const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
    
    // Check all positions in the scoring area
    for (int col : area.score_cols) {
        if (gameState.inBounds(col,area.row) &&
            gameState.isPlayerPiece(col,area.row, isCirclePlayer) &&
            gameState.getPieceType(col,area.row) == "stone") {
            count++;
        }
    }
    
    return count;
}
// Count stones in scoring area - helper function
int BoardEvaluator::countRiversInScoringArea(const GameState& gameState, bool isCirclePlayer) const {
    int count = 0;
    const ScoringArea& area = isCirclePlayer ? circle_scoring : square_scoring;
    
    // Check all positions in the scoring area
    for (int col : area.score_cols) {
        if (gameState.inBounds(col,area.row) &&
            gameState.isPlayerPiece(col,area.row, isCirclePlayer) &&
            gameState.getPieceType(col,area.row) == "river") {
            count++;
        }
    }
    
    return count;
}

// ================= DISTANCE-BASED EVALUATION ========================

// Calculate distances from scoring area for all player pieces
// Returns list of minimum Manhattan distances to any of the 4 scoring cells
// UPDATED: getmoveDistancesFromScoringArea with local river flow cache
std::vector<int> BoardEvaluator::getmoveDistancesFromScoringArea(const GameState& gameState, bool isCirclePlayer) const {        
    std::vector<int> distances;
    const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
    
    // LOCAL CACHE: Create simple vector-based cache for this evaluation
    std::vector<RiverFlowCacheEntry> river_flow_cache;
    river_flow_cache.reserve(50);  // Reserve space for typical cache size
    
    // g_logger.log(LogLevel::DEBUG, "CACHE: Starting distance evaluation with local river flow cache");
    
    for (const auto& piece : player_positions) {
        int d = distance_from_piece_with_cache(gameState, isCirclePlayer, piece, river_flow_cache);
        if (d == 0) continue; // Skip pieces already in scoring area
        distances.push_back(d);
    }

    std::sort(distances.begin(), distances.end());
    
    // g_logger.log(LogLevel::DEBUG, "CACHE: Distance evaluation complete, cache had " + 
    //             std::to_string(river_flow_cache.size()) + " entries");
    
    return distances;
}

// UPDATED: distance_from_piece with cache parameter
int BoardEvaluator::distance_from_piece_with_cache(const GameState& gameState, bool isCirclePlayer, 
                                    const std::pair<int,int>& piece,
                                    std::vector<RiverFlowCacheEntry>& river_flow_cache) const {
    // Find minimum number of moves for a piece to reach any cell of scoring area using BFS
    int piece_x = piece.first;
    int piece_y = piece.second;
    
    // Check if piece is already in scoring area
    if (gameState.isOwnScoreCell(piece_x, piece_y, isCirclePlayer)) {
        return 0;
    }

    int rows = gameState.getRows();
    int cols = gameState.getCols();

    // BFS setup
    std::queue<std::pair<std::pair<int,int>, int>> bfs_queue; // ((x,y), distance)
    // std::set<std::pair<int,int>> visited;

    // Use thread-local 2D boolean matrix for visited tracking (much faster than std::set)
    static std::vector<std::vector<bool>> visited_matrix;
    if (visited_matrix.size() != rows) {
        visited_matrix.resize(rows, std::vector<bool>(cols, false));
    } else {
        // Reset existing matrix (faster than recreating)
        for (auto& row : visited_matrix) {
            std::fill(row.begin(), row.end(), false);
        }
    }
    
    // Start BFS from piece position
    bfs_queue.push({{piece_x, piece_y}, 0});
    // visited.insert({piece_x, piece_y});
    visited_matrix[piece_y][piece_x] = true;  // Mark as visited in 2D matrix
    
    // Direction vectors for adjacent cells
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    
    // Cache statistics for debugging
    int cache_hits = 0;
    int cache_misses = 0;

    while (!bfs_queue.empty()){
        auto current = bfs_queue.front();
        bfs_queue.pop();
        
        int curr_x = current.first.first;
        int curr_y = current.first.second;
        int curr_dist = current.second;
        
        // Check all 4 adjacent positions
        for (int dir = 0; dir < 4; dir++) {
            int next_x = curr_x + dx[dir];
            int next_y = curr_y + dy[dir];
            
            // Skip if out of bounds
            if (!gameState.inBounds(next_x, next_y)) continue;
            
            // Skip if already visited
            if (visited_matrix[next_y][next_x]) continue;
            
            uint8_t next_piece = gameState.getPiece(next_x, next_y);
            
            // If it's an empty cell
            if (next_piece == EMPTY) {
                // Check if this is a scoring cell
                if (gameState.isOwnScoreCell(next_x, next_y, isCirclePlayer)) {
                    return curr_dist + 1;
                }
                
                // Add to BFS queue for further exploration
                // visited.insert({next_x, next_y});
                visited_matrix[next_y][next_x] = true;
                bfs_queue.push({{next_x, next_y}, curr_dist + 1});
            }
            // If it's a river piece, we can potentially flow through it
            else if (::isRiver(next_piece)) {
                // visited.insert({next_x, next_y});
                visited_matrix[next_y][next_x] = true;

                // OPTIMIZED: Use cached river flow instead of recomputing!
                auto flow_destinations = getCachedRiverFlow(
                    gameState, next_x, next_y, curr_x, curr_y, isCirclePlayer, 
                    river_flow_cache, cache_hits, cache_misses);

                // Check all flow destinations
                for (const auto& dest : flow_destinations) {
                    int dest_x = dest.first;
                    int dest_y = dest.second;
                    
                    // Skip if already visited
                    if (visited_matrix[dest_y][dest_x]) continue;
                    
                    // Check if this destination is a scoring cell
                    if (gameState.isOwnScoreCell(dest_x, dest_y, isCirclePlayer)) {
                        return curr_dist + 1;
                    }
                    
                    // Add to BFS queue for further exploration
                    // visited.insert({dest_x, dest_y});
                    visited_matrix[dest_y][dest_x] = true;
                    bfs_queue.push({{dest_x, dest_y}, curr_dist + 1});
                }
            }
            // If it's a stone piece, we can potentially push it (but that's complex, skip for now)
            // For simplicity, we'll only consider empty cells and river flow
        }
    }
    
    
    // Log cache statistics for this piece
    if (cache_hits + cache_misses > 0) {
        float hit_rate = (100.0f * cache_hits) / (cache_hits + cache_misses);
        // g_logger.log(LogLevel::DEBUG, "CACHE: Piece (" + std::to_string(piece_x) + "," + 
        //             std::to_string(piece_y) + ") - " + std::to_string(cache_hits) + 
        //             " hits, " + std::to_string(cache_misses) + " misses (" + 
        //             std::to_string(hit_rate) + "% hit rate)");
    }

    // If no path found to scoring area, return a large number
        return 100;
}

// Get cached river flow using simple vector lookup
std::vector<std::pair<int,int>> BoardEvaluator::getCachedRiverFlow(
    const GameState& gameState, int river_x, int river_y, int source_x, int source_y, 
    bool isCirclePlayer, std::vector<RiverFlowCacheEntry>& cache, 
    int& cache_hits, int& cache_misses) const {
    
    // STEP 1: Check cache for existing entry (O(n) lookup where n = cache size)
    for (const auto& entry : cache) {
        if (entry.river_x == river_x && entry.river_y == river_y &&
            entry.source_x == source_x && entry.source_y == source_y &&
            entry.isCirclePlayer == isCirclePlayer) {
            
            // CACHE HIT!
            cache_hits++;
            return entry.flow_destinations;
        }
    }
    
    // STEP 2: Cache miss - compute using ground truth method
    cache_misses++;
    auto flow_destinations = moveGenerator->computeRiverFlow(
        gameState, river_x, river_y, source_x, source_y, isCirclePlayer, false);
    
    // STEP 3: Store result in cache for future lookups
    cache.emplace_back(river_x, river_y, source_x, source_y, isCirclePlayer, flow_destinations);
    
    return flow_destinations;
}

// Count stones adjacent to any river (for mobility evaluation)
int BoardEvaluator::countStonesAdjacentToRivers(const GameState& gameState, bool isCirclePlayer) const {
    int mobile_stone_count = 0;
    const auto& player_positions = gameState.getPlayerPiecePositions(isCirclePlayer);
    
    // Check each player piece
    for (const auto& pos : player_positions) {
        int x = pos.first;
        int y = pos.second;
        uint8_t piece = gameState.getPiece(x, y);
        
        // Only count stones (not rivers)
        if (!::isStone(piece)) continue;
        
        // Check if this stone is adjacent to any river (in 4 directions)
        bool is_adjacent_to_river = false;
        
        // Check all 4 adjacent directions: up, down, left, right
        static const int dx[] = {0, 0, -1, 1};
        static const int dy[] = {-1, 1, 0, 0};
        
        for (int dir = 0; dir < 4; ++dir) {
            int adj_x = x + dx[dir];
            int adj_y = y + dy[dir];
            
            if (gameState.inBounds(adj_x, adj_y)) {
                uint8_t adj_piece = gameState.getPiece(adj_x, adj_y);
                
                // If adjacent cell contains any river (regardless of owner), this stone is mobile
                if (::isRiver(adj_piece)) {
                    is_adjacent_to_river = true;
                    break;  // Found at least one adjacent river, no need to check other directions
                }
            }
        }
        
        if (is_adjacent_to_river) {
            mobile_stone_count++;
        }
    }
    
    return mobile_stone_count;
}
