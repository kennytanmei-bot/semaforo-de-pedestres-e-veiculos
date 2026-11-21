/* ============================================================================
 * ARQUIVO: src/intersection.c
 * CORREÇÕES DE PREVENÇÃO DE CORTE DE FASE E LOCKUP (ti_night_cut / ts_reset_a)
 * ============================================================================ */

#include "app.h"
#include "config.h"
#include "io.h"

static VehicleState current_vehicle_state = VEHICLE_STATE_GREEN;
static uint32_t state_start_time = 0;

// Função auxiliar para garantir transições válidas de sinal
static void safe_transition_to(VehicleState new_state) {
    uint32_t now = io_get_time_ms();
    
    // CORREÇÃO (ts_reset_a): Impede a transição proibida VERDE -> VERMELHO
    if (current_vehicle_state == VEHICLE_STATE_GREEN && new_state == VEHICLE_STATE_RED) {
        current_vehicle_state = VEHICLE_STATE_YELLOW;
        state_start_time = now;
        return;
    }

    // CORREÇÃO (ti_night_cut): Impede aborto prematuro da fase amarela (mínimo 1000ms)
    if (current_vehicle_state == VEHICLE_STATE_YELLOW) {
        if (now - state_start_time < VEHICLE_YELLOW_DURATION_MS) {
            return; // Bloqueia a troca de estado até cumprir 1000ms
        }
    }

    current_vehicle_state = new_state;
    state_start_time = now;
}

void intersection_process_state(void) {
    uint32_t now = io_get_time_ms();
    uint32_t elapsed = now - state_start_time;

    // Tratamento de Perda de Sincronismo/Reset do Par (Placa B detecta Reset da Placa A)
    if (io_is_board_b() && io_check_peer_reset_detected()) {
        // CORREÇÃO (ts_reset_a): Garante tempo mínimo de Verde (3000ms) antes de chavear
        if (current_vehicle_state == VEHICLE_STATE_GREEN && elapsed < MIN_VEHICLE_GREEN_MS) {
            return; 
        }
        
        // CORREÇÃO (ti_night_cut): Destrava o estado Vermelho excedente (>5000ms)
        if (current_vehicle_state == VEHICLE_STATE_RED && elapsed >= VEHICLE_RED_DURATION_MS) {
            safe_transition_to(VEHICLE_STATE_GREEN);
            return;
        }
    }

    // Máquina de estados principal com temporização nominal
    switch (current_vehicle_state) {
        case VEHICLE_STATE_GREEN:
            if (elapsed >= MIN_VEHICLE_GREEN_MS && (io_pedestrian_requested() || io_is_phase_timeout())) {
                safe_transition_to(VEHICLE_STATE_YELLOW);
            }
            break;

        case VEHICLE_STATE_YELLOW:
            if (elapsed >= VEHICLE_YELLOW_DURATION_MS) { // 1000 ms
                safe_transition_to(VEHICLE_STATE_RED);
            }
            break;

        case VEHICLE_STATE_RED:
            if (elapsed >= VEHICLE_RED_DURATION_MS) { // 5000 ms
                safe_transition_to(VEHICLE_STATE_GREEN);
            }
            break;

        default:
            break;
    }
}
