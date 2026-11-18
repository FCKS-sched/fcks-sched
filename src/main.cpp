#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <xgboost/c_api.h>
#include <zmqpp/zmqpp.hpp>
#include <random>
#include <algorithm>

constexpr bst_ulong NUM_ACTIONS = 8;
constexpr bst_ulong STATE_DIM = 4;

#define CHECK_XGB(call)                                                     \
    if ((call) != 0) {                                                      \
        std::cerr << "XGBoost error: " << XGBGetLastError() << std::endl;   \
        return 1;                                                           \
    }

#pragma pack(push, 1)
struct Experience {
    float state[STATE_DIM];
    uint8_t action;
    float reward;
    float next_state[STATE_DIM];
    uint8_t done;
};
#pragma pack(pop)

uint8_t select_action(const float* scores, bst_ulong len, float epsilon, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (epsilon > 0.0f && dist(rng) < epsilon) {
        std::uniform_int_distribution<int> act_dist(0, static_cast<int>(NUM_ACTIONS - 1));
        return static_cast<uint8_t>(act_dist(rng));
    }

    bst_ulong count = std::min(len, NUM_ACTIONS);
    const float* max_it = std::max_element(scores, scores + count);
    return static_cast<uint8_t>(std::distance(scores, max_it));
}

bool load_booster_from_buffer(const void* data, size_t len, BoosterHandle& out_booster) {
    BoosterHandle candidate = nullptr;
    if (XGBoosterCreate(nullptr, 0, &candidate) != 0) {
        std::cerr << "XGBoosterCreate failed: " << XGBGetLastError() << std::endl;
        return false;
    }
    if (XGBoosterLoadModelFromBuffer(candidate, data, len) != 0) {
        std::cerr << "XGBoosterLoadModelFromBuffer failed: " << XGBGetLastError() << std::endl;
        XGBoosterFree(candidate);
        return false;
    }
    out_booster = candidate;
    return true;
}

int main() {
    zmqpp::context context;
    context.set(zmqpp::context_option::io_threads, 1);

    zmqpp::socket socket(context, zmqpp::socket_type::pair);
    socket.set(zmqpp::socket_option::receive_high_water_mark, 10);
    socket.set(zmqpp::socket_option::send_high_water_mark, 100);
    socket.set(zmqpp::socket_option::linger, 0);
    socket.bind("tcp://*:5555");

    std::mt19937 rng(42);
    BoosterHandle booster = nullptr;
    zmqpp::message incoming;

    socket.receive(incoming);
    if (!load_booster_from_buffer(incoming.raw_data(0), incoming.size(0), booster)) {
        return 1;
    }
    std::cout << "Initial XGBoost model loaded (" << incoming.size(0) << " bytes)" << std::endl;

    Experience pending_exp{};
    bool has_pending = false;

    constexpr char predict_config[] =
        R"({"type": 0, "training": false, "iteration_begin": 0, "iteration_end": 0, "strict_shape": false})";

    while (true) {
        float current_state[STATE_DIM] = {0.1f, 0.2f, 0.3f, 0.4f};

        if (has_pending) {
            std::memcpy(pending_exp.next_state, current_state, sizeof(current_state));
            pending_exp.reward = 1.0f;
            pending_exp.done = 0;

            zmqpp::message exp_msg;
            exp_msg.add_raw(&pending_exp, sizeof(Experience));
            socket.send(exp_msg, true);
        }

        DMatrixHandle dmat = nullptr;
        CHECK_XGB(XGDMatrixCreateFromMat(current_state, 1, STATE_DIM, NAN, &dmat));

        const bst_ulong* out_shape = nullptr;
        bst_ulong out_dim = 0;
        const float* out_result = nullptr;
        CHECK_XGB(XGBoosterPredictFromDMatrix(
            booster, dmat, predict_config, &out_shape, &out_dim, &out_result));

        bst_ulong out_len = (out_dim > 0) ? 1 : 0;
        for (bst_ulong i = 0; i < out_dim; ++i) {
            out_len *= out_shape[i];
        }

        float epsilon = 0.05f;
        uint8_t action = select_action(out_result, out_len, epsilon, rng);
        XGDMatrixFree(dmat);

        //apply policy change

        std::memcpy(pending_exp.state, current_state, sizeof(current_state));
        pending_exp.action = action;
        has_pending = true;

        bool has_update = false;
        while (socket.receive(incoming, true)) {
            has_update = true;
        }

        if (has_update) {
            BoosterHandle next_booster = nullptr;
            if (load_booster_from_buffer(incoming.raw_data(0), incoming.size(0), next_booster)) {
                BoosterHandle old_booster = booster;
                booster = next_booster;
                XGBoosterFree(old_booster);
                std::cout << "Swapped to updated model (" << incoming.size(0) << " bytes)" << std::endl;
            }
        }
    }
}
