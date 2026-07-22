
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "eval/eval7.hpp"
#include "game/card.hpp"

using cfr::Card;
using cfr::kCardCount;
using cfr::eval::evaluate_7card;
using cfr::eval::evaluate_7card_batch;
using cfr::eval::HandRank;

namespace {

constexpr std::size_t kHandCount = 10'000'000;
constexpr int kCardsPerHand = 7;

constexpr std::uint64_t kRandomSeed = 0xC5F7BE4C4;

std::array<Card, kCardsPerHand> random_seven_card_hand(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> card_distribution(0, kCardCount - 1);
    std::array<Card, kCardsPerHand> hand;
    std::array<bool, kCardCount> used = {};
    for (Card& card : hand) {
        int drawn;
        do {
            drawn = card_distribution(rng);
        } while (used[drawn]);
        used[drawn] = true;
        card = static_cast<Card>(drawn);
    }
    return hand;
}

std::vector<Card> generate_hand_buffer() {
    std::mt19937_64 rng(kRandomSeed);
    std::vector<Card> hands(kHandCount * kCardsPerHand);
    for (std::size_t hand_index = 0; hand_index < kHandCount; ++hand_index) {
        std::array<Card, kCardsPerHand> hand = random_seven_card_hand(rng);
        std::copy(hand.begin(), hand.end(), hands.begin() + static_cast<std::ptrdiff_t>(hand_index * kCardsPerHand));
    }
    return hands;
}

std::string cpu_model() {
    FILE* sysctl_output = popen("sysctl -n machdep.cpu.brand_string", "r");
    if (sysctl_output == nullptr) return "unknown";

    std::string brand_string;
    char line_buffer[256];
    while (std::fgets(line_buffer, sizeof(line_buffer), sysctl_output) != nullptr) {
        brand_string += line_buffer;
    }
    pclose(sysctl_output);

    while (!brand_string.empty() && (brand_string.back() == '\n' || brand_string.back() == '\r')) {
        brand_string.pop_back();
    }
    return brand_string.empty() ? "unknown" : brand_string;
}

double hands_per_second(std::size_t hand_count, std::chrono::steady_clock::duration elapsed) {
    double elapsed_seconds = std::chrono::duration<double>(elapsed).count();
    return static_cast<double>(hand_count) / elapsed_seconds;
}

}

int main() {
    std::printf("CPU: %s\n", cpu_model().c_str());
    std::printf("evaluate_7card: 21-subset scalar reference, no vectorized path\n");

    std::vector<Card> hands = generate_hand_buffer();
    std::vector<HandRank> ranks(kHandCount);

    std::uint64_t scalar_checksum = 0;
    auto scalar_start = std::chrono::steady_clock::now();
    for (std::size_t hand_index = 0; hand_index < kHandCount; ++hand_index) {
        ranks[hand_index] = evaluate_7card(hands.data() + hand_index * kCardsPerHand);
        scalar_checksum ^= ranks[hand_index];
    }
    auto scalar_elapsed = std::chrono::steady_clock::now() - scalar_start;

    auto batch_start = std::chrono::steady_clock::now();
    evaluate_7card_batch(hands.data(), kHandCount, ranks.data());
    auto batch_elapsed = std::chrono::steady_clock::now() - batch_start;

    std::uint64_t batch_checksum = 0;
    for (std::size_t hand_index = 0; hand_index < kHandCount; ++hand_index) {
        batch_checksum ^= ranks[hand_index];
    }

    std::printf("hands evaluated: %zu\n", kHandCount);
    std::printf("scalar evaluate_7card: %.0f hands/sec (checksum: 0x%016llx)\n",
                 hands_per_second(kHandCount, scalar_elapsed), static_cast<unsigned long long>(scalar_checksum));
    std::printf("evaluate_7card_batch:  %.0f hands/sec (checksum: 0x%016llx)\n",
                 hands_per_second(kHandCount, batch_elapsed), static_cast<unsigned long long>(batch_checksum));
    return 0;
}
