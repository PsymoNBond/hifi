//
//  AACubeTests.cpp
//  tests/shared/src
//
//  Created by Andrew Meadows on 2016.02.19
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AACubeTests.h"

#include <iostream>

#include <GLMHelpers.h>
#include <NumericalConstants.h>
#include <StreamUtils.h>

#include <test-utils/GLMTestUtils.h>
#include <test-utils/QTestExtensions.h>


QTEST_MAIN(AACubeTests)

void AACubeTests::ctorsAndSetters() {
    const glm::vec3 corner(1.23f, 4.56f, 7.89f);
    const float scale = 2.34f;

    // test ctor
    AACube cube(corner, scale);
    QCOMPARE_WITH_ABS_ERROR(cube.getCorner(), corner, EPSILON);
    QCOMPARE_WITH_ABS_ERROR(cube.getScale(), scale, EPSILON);

    // test copy ctor
    AACube copyCube(cube);
    QCOMPARE_WITH_ABS_ERROR(copyCube.getCorner(), corner, EPSILON);
    QCOMPARE_WITH_ABS_ERROR(copyCube.getScale(), scale, EPSILON);

    // test setBox()
    const glm::vec3 newCorner(9.87f, 6.54f, 3.21f);
    const float newScale = 4.32f;
    cube.setBox(newCorner, newScale);
    QCOMPARE_WITH_ABS_ERROR(cube.getCorner(), newCorner, EPSILON);
    QCOMPARE_WITH_ABS_ERROR(cube.getScale(), newScale, EPSILON);

    // test misc
    QCOMPARE_WITH_ABS_ERROR(cube.getMinimumPoint(), newCorner, EPSILON);

    glm::vec3 expectedMaxCorner = newCorner + glm::vec3(newScale);
    QCOMPARE_WITH_ABS_ERROR(cube.getMaximumPoint(), expectedMaxCorner, EPSILON);

    glm::vec3 expectedCenter = newCorner + glm::vec3(0.5f * newScale);
    QCOMPARE_WITH_ABS_ERROR(cube.calcCenter(), expectedCenter, EPSILON);
}

void AACubeTests::containsPoint() {
    const glm::vec3 corner(4.56f, 7.89f, -1.35f);
    const float scale = 1.23f;
    AACube cube(corner, scale);

    const float delta = scale / 1000.0f;
    const glm::vec3 center = cube.calcCenter();
    QCOMPARE(cube.contains(center), true);

    for (int i = 0; i < 3; ++i) {
        glm::vec3 scaleOffset = Vectors::ZERO;
        scaleOffset[i] = 0.5f * scale;

        glm::vec3 deltaOffset = Vectors::ZERO;
        deltaOffset[i] = delta;

        QCOMPARE(cube.contains(center + scaleOffset + deltaOffset), false); // outside +face
        QCOMPARE(cube.contains(center + scaleOffset - deltaOffset), true); // inside +face
        QCOMPARE(cube.contains(center - scaleOffset + deltaOffset), true); // inside -face
        QCOMPARE(cube.contains(center - scaleOffset - deltaOffset), false); // outside -face
    }
}

void AACubeTests::touchesSphere() {
    const glm::vec3 corner(-4.56f, 7.89f, -1.35f);
    const float scale = 1.23f;
    AACube cube(corner, scale);

    const float delta = scale / 1000.0f;
    const glm::vec3 cubeCenter = cube.calcCenter();
    const float sphereRadius = 0.468f;

    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        int k = (j + 1) % 3;

        { // faces
            glm::vec3 scaleOffset = Vectors::ZERO;
            scaleOffset[i] = 0.5f * scale + sphereRadius;

            glm::vec3 deltaOffset = Vectors::ZERO;
            deltaOffset[i] = delta;

            // outside +face
            glm::vec3 sphereCenter = cubeCenter + scaleOffset + deltaOffset;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);

            // inside +face
            sphereCenter = cubeCenter + scaleOffset - deltaOffset;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);

            // inside -face
            sphereCenter = cubeCenter - scaleOffset + deltaOffset;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);

            // outside -face
            sphereCenter = cubeCenter - scaleOffset - deltaOffset;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);
        }

        { // edges
            glm::vec3 edgeOffset = Vectors::ZERO;
            edgeOffset[i] = 0.5f * scale;
            edgeOffset[j] = 0.5f * scale;
            glm::vec3 edgeDirection = glm::normalize(edgeOffset);
            glm::vec3 sphereCenter;

            // inside ij
            sphereCenter = cubeCenter + edgeOffset + (sphereRadius - delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);
            sphereCenter = cubeCenter - edgeOffset - (sphereRadius - delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);

            // outside ij
            sphereCenter = cubeCenter + edgeOffset + (sphereRadius + delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);
            sphereCenter = cubeCenter - edgeOffset - (sphereRadius + delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);

            edgeOffset[j] = 0.0f;
            edgeOffset[k] = 0.5f * scale;
            edgeDirection = glm::normalize(edgeOffset);

            // inside ik
            sphereCenter = cubeCenter + edgeOffset + (sphereRadius - delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);
            sphereCenter = cubeCenter - edgeOffset - (sphereRadius - delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), true);

            // outside ik
            sphereCenter = cubeCenter + edgeOffset + (sphereRadius + delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);
            sphereCenter = cubeCenter - edgeOffset - (sphereRadius + delta) * edgeDirection;
            QCOMPARE(cube.touchesSphere(sphereCenter, sphereRadius), false);
        }
    }
}

void AACubeTests::rayVsParabolaPerformance() {
    // Test performance of findRayIntersection vs. findParabolaIntersection
    // 100000 cubes with scale 500 in the +x +y +z quadrant
    const int NUM_CUBES = 100000;
    const float MAX_POS = 1000.0f;
    const float MAX_SCALE = 500.0f;
    int numRayHits = 0;
    int numParabolaHits = 0;
    std::vector<AACube> cubes;
    cubes.reserve(NUM_CUBES);
    for (int i = 0; i < NUM_CUBES; i++) {
        cubes.emplace_back(glm::vec3(randFloatInRange(0.0f, MAX_POS), randFloatInRange(0.0f, MAX_POS), randFloatInRange(0.0f, MAX_POS)), MAX_SCALE);
    }

    glm::vec3 origin(0.0f);
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f));
    glm::vec3 invDirection = 1.0f / direction;
    float distance;
    BoxFace face;
    glm::vec3 normal;
    auto start = std::chrono::high_resolution_clock::now();
    for (auto& cube : cubes) {
        if (cube.findRayIntersection(origin, direction, invDirection, distance, face, normal)) {
            numRayHits++;
        }
    }

    auto rayTime = std::chrono::high_resolution_clock::now() - start;
    start = std::chrono::high_resolution_clock::now();
    direction = 10.0f * direction;
    glm::vec3 acceleration = glm::vec3(-0.0001f, -0.0001f, -0.0001f);
    for (auto& cube : cubes) {
        if (cube.findParabolaIntersection(origin, direction, acceleration, distance, face, normal)) {
            numParabolaHits++;
        }
    }
    auto parabolaTime = std::chrono::high_resolution_clock::now() - start;

    qDebug() << "Ray vs. Parabola perfomance: rayHit%:" << numRayHits / ((float)NUM_CUBES) * 100.0f << ", rayTime:" << rayTime.count() <<
        ", parabolaHit%:" << numParabolaHits / ((float)NUM_CUBES) * 100.0f << ", parabolaTime:" << parabolaTime.count() << ", parabolaTime/rayTime: " << (float)parabolaTime.count()/(float)rayTime.count();
}

void AACubeTests::cleanupTestCase() {

}