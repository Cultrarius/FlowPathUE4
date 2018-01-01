//
// Created by Michael Galetzka on 01.01.2018.
//
// The algorithm for this eikonal solver is taken from the following paper:
// https://www.cv-foundation.org/openaccess/content_iccv_2015/papers/Cancela_A_Wavefront_Marching_ICCV_2015_paper.pdf
//

#include "WaveMarcher.h"
#include <map>

using namespace flow;
using namespace std;

const int MAX_VAL = 255;
const int S_RIGHT = 0;
const int S_LEFT = 1;

enum class State {
    Sleeping, Alive, Trial
};

struct WaveNode {
    float value = MAX_VAL;
    State state = State::Sleeping;
};

struct Wmm {
    FIntPoint p;
    float v[3];
    float m[5];
    float fm[5];
    uint8 dir = -1;
};

int32 toIndex(const FIntPoint& coordinates, int length)
{
    return coordinates.X + coordinates.Y * length;
}

bool isValidLocation(const FIntPoint & p, int32 length)
{
    return p.X >= 0 && p.Y >= 0 && p.X < length && p.Y < length;
}

void setCoefficients(const float* y, float* m, int pos) {
    m[0] = y[pos];
    if (pos % 2 == 0) {
        m[2] = (y[(pos + 2) % 8] + y[pos] - 2.0 * y[(pos + 1) % 8]) / 2.0;
        m[1] = y[(pos + 1) % 8] - y[pos] - m[2];
        m[4] = (y[(pos + 6) % 8] + y[pos] - 2.0 * y[(pos + 7) % 8]) / 2.0;
        m[3] = y[(pos + 7) % 8] - y[pos] - m[4];
    }
    else {
        m[2] = m[4] = (y[(pos + 1) % 8] + y[(pos + 7) % 8] - 2.0 * y[pos]) / 2.0;
        m[1] = y[pos] - y[(pos + 7) % 8] + m[2];
        m[3] = y[pos] - y[(pos + 1) % 8] + m[4];
    }
}

typename multimap<float, Wmm>::iterator trialSet_it;
typename map<int, typename multimap<float, Wmm >::iterator>::iterator mapa_trial_it;

float interpolateValue(const Wmm &wave, const FIntPoint& wavePoint, const FVector2D &dd, const FIntPoint &neighbor, const FVector2D &f0,
    const FVector2D &f1, const FVector2D &fn, float epsilon, int side) {

    FVector2D dp(wavePoint.X, wavePoint.Y);
    FVector2D dn(neighbor.X, neighbor.Y);
    float y0 = wave.v[0];
    float y1 = wave.v[side + 1];

    float ft = wave.fm[2 * side + 2] * epsilon * epsilon + wave.fm[2 * side + 1] * epsilon + wave.fm[0];
    float norm = (dn - dp - epsilon * dd).Size();
    float value = wave.m[2 * side + 2] * epsilon * epsilon + wave.m[2 * side + 1] * epsilon + wave.m[0] + norm * (ft + fn.Size()) / 2.0;
    if (value < y0) {
        float v0 = (1.0 - epsilon) * y0;
        float v1 = epsilon * y1;
        value = v0 + v1 + norm * ((1.0 - epsilon) * f0.Size() + epsilon * f1.Size() + fn.Size()) / 2.0;
    }

    return value;
}

float calculateEpsilonGradient(const FVector2D &dd, const FVector2D &dp, const FVector2D &dn, float fn) {
    float epsilon;
    float A = -dd.Y;
    float B = dd.X;
    float C = dd.Y * dp.X - dd.X * dp.Y;
    float den = B * fn;
    float t = (A * dn.X + B * dn.Y + C) / den;

    FVector2D x(dn.Y - t * fn, dn.X);
    if (fabs(dd.X) > 0.0 && fabs(den) > 0.0) {
        epsilon = (x.X - dp.X) / dd.X;
    }
    else if (fabs(dd.Y) > 0.0 && fabs(den) > 0.0) {
        epsilon = (x.Y - dp.Y) / dd.Y;
    }
    else if (fabs(den) == 0.0 && dd.Size() > 0.0) {
        float dist = fabs(A * dn.X + B * dn.Y + C) / sqrt(A * A + B * B);
        epsilon = ((dn - dp).Size() - dist) / (fabs(dd.X) + fabs(dd.Y));
    }
    else {
        return 0;
    }
    return FMath::Clamp(epsilon, 0.0f, 1.0f);
}

float getVal2D(const TArray<uint8>& sourceData, const TArray<WaveNode> &waveSurface, const Wmm &wave, const FIntPoint &neighbor, int32 length) {
    FVector2D fn(sourceData[toIndex(neighbor, length)], 0);
    FVector2D f0 = isValidLocation(wave.p, length) ? FVector2D(sourceData[toIndex(wave.p, length)], 0) : fn;
    float y0 = wave.v[0];
    float val;
    if (wave.dir < 0) {
        FVector2D diff = neighbor - wave.p;
        val = y0 + diff.Size() * (f0.Size() + fn.Size()) / 2.0;
    }
    else {
        FIntPoint p(wave.p.Y + yarray[(wave.dir + 1) % 8] - yarray[wave.dir], wave.p.X + xarray[(wave.dir + 1) % 8] - xarray[wave.dir]);
        float res1 = MAX_VAL;

        if (isValidLocation(p, length)) {
            FVector2D dd(yarray[(wave.dir + 1) % 8] - yarray[wave.dir], xarray[(wave.dir + 1) % 8] - xarray[wave.dir]);
            FVector2D f1(sourceData[toIndex(p, length)], 0);
            float epsilon = calculateEpsilonGradient(dd, wave.p, neighbor, fn.X);
            res1 = interpolateValue(wave, wave.p, dd, neighbor, f0, f1, fn, epsilon, S_RIGHT);
        }

        p = FIntPoint(wave.p.Y + yarray[(wave.dir + 7) % 8] - yarray[wave.dir], wave.p.X + xarray[(wave.dir + 7) % 8] - xarray[wave.dir]);
        float res2 = MAX_VAL;

        if (isValidLocation(p, length)) {
            FVector2D dd(yarray[(wave.dir + 7) % 8] - yarray[wave.dir], xarray[(wave.dir + 7) % 8] - xarray[wave.dir]);
            FVector2D f1(sourceData[toIndex(p, length)], 0);
            float epsilon = calculateEpsilonGradient(dd, wave.p, neighbor, fn.X);
            res2 = interpolateValue(wave, wave.p, dd, neighbor, f0, f1, fn, epsilon, S_LEFT);

        }

        val = FMath::Min(res1, res2);
    }

    return val;
}

void flow::CreateEikonalSurface(const TArray<uint8>& sourceData, const TArray<FIntPoint> targetPoints, TArray<float>* output)
{
    int32 length = FMath::Sqrt(sourceData.Num());
    check(length);
    output->Empty(sourceData.Num());
    TArray<WaveNode> waveSurface;
    waveSurface.AddDefaulted(sourceData.Num());
    
    // Data definitions
    bool isnewpos[8];
    float valcenter[8];
    float imcenter[8];
    multimap<float, Wmm> trialSet;
    map<int32, typename multimap<float, Wmm>::iterator> mapaTrial;
    pair<float, Wmm> trialPair;
    pair<int32, typename multimap<float, Wmm>::iterator> mapaPair;
    Wmm winner, newWave;

    // Target point initialization
    for (int32 i = 0; i < targetPoints.Num(); i++) {
        FIntPoint target = targetPoints[i];
        check(target.X >= 0 && target.X < length);
        check(target.Y >= 0 && target.Y < length);
        int32 index = toIndex(target, length);
        if (mapaTrial.find(index) == mapaTrial.end()) {
            waveSurface[index].value = 0.0;
            waveSurface[index].state = State::Trial;
            winner.dir = -1;
            winner.v[0] = 0.0;
            winner.p = target;
            trialPair = pair<float, Wmm>(0.0, winner);
            trialSet_it = trialSet.insert(trialPair);
            mapaPair = pair<int, typename multimap<float, Wmm>::iterator>(index, trialSet_it);
            mapaTrial.insert(mapaPair);
        }
    }

    // Loop until all nodes are processed
    while (!trialSet.empty()) {
        trialSet_it = trialSet.begin();
        int32 index = toIndex(trialSet_it->second.p, length);
        mapa_trial_it = mapaTrial.find(index);

        // Map allocation checks
        check(mapa_trial_it != mapaTrial.end());
        check(mapa_trial_it->second == trialSet_it);

        winner = trialSet_it->second;

        trialSet.erase(trialSet_it);
        mapaTrial.erase(mapa_trial_it);
        waveSurface[index].state = State::Alive;

        // Neighbor value computation
        for (int32 i = 0; i < 8; i++) {
            FIntPoint neighbor = winner.p + neighbors[i];
            bool isValid = isValidLocation(neighbor, length);
            int32 index = toIndex(neighbor, length);
            isnewpos[i] = false;
            valcenter[i] = isValid ? waveSurface[index].value : MAX_VAL;
            imcenter[i] = isValid ? sourceData[index] : MAX_VAL;
            if (isValid && waveSurface[index].state != State::Alive) {
                float val_neigh = getVal2D(sourceData, waveSurface, winner, neighbor, length);
                if (val_neigh < valcenter[i]) {
                    valcenter[i] = val_neigh;
                    isnewpos[i] = true;
                }
            }
        }

        // Update
        for (int32 i = 0; i < 8; i++) {
            if (isnewpos[i]) {
                FIntPoint neighbor = winner.p + neighbors[i];
                int32 index = toIndex(neighbor, length);
                if (waveSurface[index].state == State::Trial) {
                    mapa_trial_it = mapaTrial.find(index);
                    trialSet.erase(mapa_trial_it->second);
                    mapaTrial.erase(mapa_trial_it);
                }
                else {
                    waveSurface[index].state = State::Trial;
                }
                newWave.p = neighbor;
                newWave.dir = i;

                newWave.v[0] = valcenter[i];
                newWave.v[1] = valcenter[(i + 1) % 8];
                newWave.v[2] = valcenter[(i + 7) % 8];

                setCoefficients(valcenter, newWave.m, i);
                setCoefficients(imcenter, newWave.fm, i);

                trialPair = pair<float, Wmm >(valcenter[i], newWave);
                trialSet_it = trialSet.insert(trialPair);
                mapaPair = pair<int32, typename multimap<float, Wmm >::iterator>(
                    index, trialSet_it);
                mapaTrial.insert(mapaPair);

                waveSurface[index].value = valcenter[i];
            }
        }
    }
}
