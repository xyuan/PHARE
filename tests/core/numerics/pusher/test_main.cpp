#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "data/particles/particle_array.h"
#include "numerics/pusher/boris.h"
#include "utilities/particle_selector/particle_selector.h"
#include "utilities/range/range.h"

using namespace PHARE;


struct Trajectory
{
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<float> vx;
    std::vector<float> vy;
    std::vector<float> vz;

    Trajectory(std::size_t size)
        : x(size)
        , y(size)
        , z(size)
        , vx(size)
        , vy(size)
        , vz(size)
    {
    }
};


Trajectory readExpectedTrajectory()
{
    std::ifstream file{"pusher_test_in.txt"};

    std::vector<double> data;

    std::copy(std::istream_iterator<double>(file), std::istream_iterator<double>(),
              std::back_inserter(data));

    auto nlines = data.size() / 6;
    Trajectory traj(nlines);

    for (auto iline = 0u; iline < nlines; ++iline)
    {
        traj.x[iline]  = static_cast<float>(data[iline * 6]);
        traj.y[iline]  = static_cast<float>(data[iline * 6 + 1]);
        traj.z[iline]  = static_cast<float>(data[iline * 6 + 2]);
        traj.vx[iline] = static_cast<float>(data[iline * 6 + 3]);
        traj.vy[iline] = static_cast<float>(data[iline * 6 + 4]);
        traj.vz[iline] = static_cast<float>(data[iline * 6 + 5]);
    }

    return traj;
}


// this is a mock of a true Interpolator
// we hard-code the fields that the particles will see to the value used
// in the python script that generates a trajectory
// this way, we don't need a proper Electromag, VecField, Field etc. objects
// to test the pusher.
class Interpolator
{
public:
    template<typename PartIterator, typename Electromag>
    void operator()(PartIterator begin, PartIterator end, Electromag const& em)
    {
        for (auto currPart = begin; currPart != end; ++currPart)
        {
            currPart->Ex = 0.01;
            currPart->Ey = -0.05;
            currPart->Ez = 0.05;
            currPart->Bx = 1.;
            currPart->By = 1.;
            currPart->Bz = 1.;
        }
    }
};


// mock of electromag just so that the Pusher gives something to
// the Interpolator
class Electromag
{
};


// with this mock, all particles are found inside
class DummySelector
{
public:
    template<typename Particle>
    bool operator()(Particle) const
    {
        return true;
    }
};

/*
class BoundaryCondition
{
public:
    template<typename ParticleIterator>
    ParticleIterator applyOutgoingParticleBC(ParticleIterator begin, ParticleIterator end)
    {
        return end;
    }
};*/


class APusher3D : public ::testing::Test
{
public:
    APusher3D()
        : expectedTrajectory{readExpectedTrajectory()}
        , particlesIn(1)
        , particlesOut(1)
        , pusher{std::make_unique<BorisPusher<3>>()}
        , mass{1}
        , dt{0.0001}
        , tstart{0}
        , tend{10}
        , nt{static_cast<std::size_t>((tend - tstart) / dt + 1)}
        , xActual(nt)
        , yActual(nt)
        , zActual(nt)
    {
        particlesIn[0].charge = 1;
        particlesIn[0].iCell  = {{5, 5, 5}}; // arbitrary we don't care
        particlesIn[0].v      = {{0, 10., 0}};
        particlesIn[0].delta  = {{0.0, 0.0, 0.0}};
        pusher->setMeshAndTimeStep({{dx, dy, dz}}, dt);
    }


protected:
    Trajectory expectedTrajectory;
    ParticleArray<3> particlesIn;
    ParticleArray<3> particlesOut;
    std::unique_ptr<BorisPusher<3>> pusher;
    double mass;
    double dt;
    double tstart;
    double tend;
    std::size_t nt;
    Electromag em;
    Interpolator interpolator;
    DummySelector selector;
    // BoundaryCondition bc;
    std::vector<float> xActual;
    std::vector<float> yActual;
    std::vector<float> zActual;
    double dx = 0.05;
    double dy = 0.05;
    double dz = 0.05;
};



TEST_F(APusher3D, trajectoryIsOk)
{
    auto rangeIn  = makeRange(std::begin(particlesIn), std::end(particlesIn));
    auto rangeOut = makeRange(std::begin(particlesOut), std::end(particlesOut));
    std::copy(rangeIn.begin(), rangeIn.end(), rangeOut.begin());

    for (decltype(nt) i = 0; i < nt; ++i)
    {
        xActual[i] = (particlesOut[0].iCell[0] + particlesOut[0].delta[0]) * static_cast<float>(dx);
        yActual[i] = (particlesOut[0].iCell[1] + particlesOut[0].delta[1]) * static_cast<float>(dy);
        zActual[i] = (particlesOut[0].iCell[2] + particlesOut[0].delta[2]) * static_cast<float>(dz);

        pusher->move(rangeIn, rangeOut, em, mass, interpolator, selector);

        std::copy(rangeOut.begin(), rangeOut.end(), rangeIn.begin());
    }

    EXPECT_THAT(xActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.x));
    EXPECT_THAT(yActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.y));
    EXPECT_THAT(zActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.z));
}



class APusher2D : public ::testing::Test
{
public:
    APusher2D()
        : expectedTrajectory{readExpectedTrajectory()}
        , particlesIn(1)
        , particlesOut(1)
        , pusher{std::make_unique<BorisPusher<2>>()}
        , mass{1}
        , dt{0.0001}
        , tstart{0}
        , tend{10}
        , nt{static_cast<std::size_t>((tend - tstart) / dt + 1)}
        , xActual(nt)
        , yActual(nt)
    {
        particlesIn[0].charge = 1;
        particlesIn[0].iCell  = {{5, 5}}; // arbitrary we don't care
        particlesIn[0].v      = {{0, 10., 0}};
        particlesIn[0].delta  = {{0.0, 0.0}};
        pusher->setMeshAndTimeStep({{dx, dy}}, dt);
    }


protected:
    Trajectory expectedTrajectory;
    ParticleArray<2> particlesIn;
    ParticleArray<2> particlesOut;
    std::unique_ptr<BorisPusher<2>> pusher;
    double mass;
    double dt;
    double tstart;
    double tend;
    std::size_t nt;
    Electromag em;
    Interpolator interpolator;
    DummySelector selector;
    std::vector<float> xActual;
    std::vector<float> yActual;
    double dx = 0.05;
    double dy = 0.05;
};



TEST_F(APusher2D, trajectoryIsOk)
{
    auto rangeIn  = makeRange(std::begin(particlesIn), std::end(particlesIn));
    auto rangeOut = makeRange(std::begin(particlesOut), std::end(particlesOut));
    std::copy(rangeIn.begin(), rangeIn.end(), rangeOut.begin());

    for (decltype(nt) i = 0; i < nt; ++i)
    {
        xActual[i] = (particlesOut[0].iCell[0] + particlesOut[0].delta[0]) * static_cast<float>(dx);
        yActual[i] = (particlesOut[0].iCell[1] + particlesOut[0].delta[1]) * static_cast<float>(dy);

        pusher->move(rangeIn, rangeOut, em, mass, interpolator, selector);

        std::copy(rangeOut.begin(), rangeOut.end(), rangeIn.begin());
    }

    EXPECT_THAT(xActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.x));
    EXPECT_THAT(yActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.y));
}



class APusher1D : public ::testing::Test
{
public:
    APusher1D()
        : expectedTrajectory{readExpectedTrajectory()}
        , particlesIn(1)
        , particlesOut(1)
        , pusher{std::make_unique<BorisPusher<1>>()}
        , mass{1}
        , dt{0.0001}
        , tstart{0}
        , tend{10}
        , nt{static_cast<std::size_t>((tend - tstart) / dt + 1)}
        , xActual(nt)
    {
        particlesIn[0].charge = 1;
        particlesIn[0].iCell  = {{5}}; // arbitrary we don't care
        particlesIn[0].v      = {{0, 10., 0}};
        particlesIn[0].delta  = {{0.0}};
        pusher->setMeshAndTimeStep({{dx}}, dt);
    }


protected:
    Trajectory expectedTrajectory;
    ParticleArray<1> particlesIn;
    ParticleArray<1> particlesOut;
    std::unique_ptr<BorisPusher<1>> pusher;
    double mass;
    double dt;
    double tstart;
    double tend;
    std::size_t nt;
    Electromag em;
    Interpolator interpolator;
    DummySelector selector;
    std::vector<float> xActual;
    double dx = 0.05;
};



TEST_F(APusher1D, trajectoryIsOk)
{
    auto rangeIn  = makeRange(std::begin(particlesIn), std::end(particlesIn));
    auto rangeOut = makeRange(std::begin(particlesOut), std::end(particlesOut));
    std::copy(rangeIn.begin(), rangeIn.end(), rangeOut.begin());

    for (decltype(nt) i = 0; i < nt; ++i)
    {
        xActual[i] = (particlesOut[0].iCell[0] + particlesOut[0].delta[0]) * static_cast<float>(dx);

        pusher->move(rangeIn, rangeOut, em, mass, interpolator, selector);

        std::copy(rangeOut.begin(), rangeOut.end(), rangeIn.begin());
    }

    EXPECT_THAT(xActual, ::testing::Pointwise(::testing::DoubleNear(1e-5), expectedTrajectory.x));
}




class APusherWithLeavingParticles : public ::testing::Test
{
public:
    APusherWithLeavingParticles()
        : particlesIn(1000)
        , pusher{std::make_unique<BorisPusher<1>>()}
        , mass{1}
        , dt{0.0001}
        , tstart{0}
        , tend{10}
        , nt{static_cast<std::size_t>((tend - tstart) / dt + 1)}
        , domain{Point<int, 1>{0}, Point<int, 1>{1}}
        , selector{domain}
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1);
        std::uniform_real_distribution<float> delta(0, 1);

        for (auto& part : particlesIn)
        {
            part.charge = 1;
            part.v      = {{0, 10., 0.}};
            part.delta  = {{delta(gen)}};
            part.iCell  = {{dis(gen)}};
        }
        pusher->setMeshAndTimeStep({{dx}}, dt);
    }


protected:
    ParticleArray<1> particlesIn;
    std::unique_ptr<BorisPusher<1>> pusher;
    double mass;
    double dt;
    double tstart;
    double tend;
    std::size_t nt;
    Electromag em;
    Interpolator interpolator;
    Box<int, 1> domain;
    ParticleSelector<Box<int, 1>> selector;
    double dx = 0.05;
};




TEST_F(APusherWithLeavingParticles, splitLeavingFromNonLeavingParticles)
{
    auto rangeIn = makeRange(std::begin(particlesIn), std::end(particlesIn));

    auto newEnd = std::end(particlesIn);

    for (decltype(nt) i = 0; i < nt; ++i)
    {
        newEnd = pusher->move(rangeIn, rangeIn, em, mass, interpolator, selector);
        if (newEnd != std::end(particlesIn))
        {
            std::cout << "stopping itnegration at i = " << i << "\n";
            std::cout << std::distance(std::begin(particlesIn), newEnd) << " in domain\n";
            std::cout << std::distance(newEnd, std::end(particlesIn)) << " leaving\n";
            break;
        }
    }
    EXPECT_TRUE(std::none_of(newEnd, std::end(particlesIn), selector));
    EXPECT_TRUE(std::all_of(std::begin(particlesIn), newEnd, selector));
}




int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
