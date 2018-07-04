#ifndef PHARE_PARTICLES_DATA_SPLIT_ON_COARSE_BOUNDARY_H
#define PHARE_PARTICLES_DATA_SPLIT_ON_COARSE_BOUNDARY_H

#include "data/particles/particles_data.h"
#include "split.h"
#include "tools/amr_utils.h"

#include <SAMRAI/geom/CartesianPatchGeometry.h>
#include <SAMRAI/hier/Box.h>
#include <SAMRAI/hier/RefineOperator.h>
#include <SAMRAI/pdat/CellOverlap.h>

#include <functional>

namespace PHARE
{
template<std::size_t interpOrder>
std::size_t constexpr ghostWidthForParticles()
{
    return interpOrder % 2 == 0 ? interpOrder / 2 + 1 : (interpOrder + 1) / 2;
}

enum class ParticlesDataSplitType { coarseBoundary, interior, coarseBoundary1, coarseBoundary2 };

std::string inline splitName(ParticlesDataSplitType splitType)
{
    switch (splitType)
    {
        case ParticlesDataSplitType::coarseBoundary: return "coarseBoundary";
        case ParticlesDataSplitType::interior: return "interior";
        case ParticlesDataSplitType::coarseBoundary1: return "coarseBoundary1";
        case ParticlesDataSplitType::coarseBoundary2: return "coarseBoundary2";
        default: throw std::runtime_error("End of enum class possible range");
    }
}

template<std::size_t dim, std::size_t interpOrder, ParticlesDataSplitType splitType>
class ParticlesDataSplitOperator : public SAMRAI::hier::RefineOperator
{
public:
    ParticlesDataSplitOperator()
        : SAMRAI::hier::RefineOperator{"ParticlesDataSplit_" + splitName(splitType)}
    {
    }

    virtual ~ParticlesDataSplitOperator() = default;

    /** @brief a priority of 0 means that this operator
     * will be applied first
     */
    int getOperatorPriority() const { return 0; }




    SAMRAI::hier::IntVector getStencilWidth(SAMRAI::tbox::Dimension const& dimension) const
    {
        return SAMRAI::hier::IntVector{dimension, ghostWidthForParticles<interpOrder>()};
    }




    /** @brief perform a split and keep those that are inside a fineOverlap
     *
     */
    void refine(SAMRAI::hier::Patch& destination, SAMRAI::hier::Patch const& source,
                int const destinationComponent, int const sourceComponent,
                SAMRAI::hier::BoxOverlap const& fineOverlap,
                SAMRAI::hier::IntVector const& ratio) const
    {
        auto const& destinationFieldOverlap
            = dynamic_cast<SAMRAI::pdat::CellOverlap const&>(fineOverlap);


        auto destinationParticlesData = std::dynamic_pointer_cast<ParticlesData<dim>>(
            destination.getPatchData(destinationComponent));

        auto const sourceParticlesData
            = std::dynamic_pointer_cast<ParticlesData<dim>>(source.getPatchData(sourceComponent));

        auto pGeom = std::dynamic_pointer_cast<SAMRAI::geom::CartesianPatchGeometry>(
            destination.getPatchGeometry());



        TBOX_ASSERT(destinationParticlesData);
        TBOX_ASSERT(sourceParticlesData);
        TBOX_ASSERT(pGeom);



        refine_(*destinationParticlesData, *sourceParticlesData, destinationFieldOverlap, ratio,
                *pGeom);
    }




    void refine_(ParticlesData<dim>& destinationParticlesData,
                 ParticlesData<dim> const& sourceParticlesData,
                 SAMRAI::pdat::CellOverlap const& destinationFieldOverlap,
                 SAMRAI::hier::IntVector const& ratio,
                 SAMRAI::geom::CartesianPatchGeometry const& pGeom) const
    {
        auto const& destinationBoxes = destinationFieldOverlap.getDestinationBoxContainer();



        auto const& sourceInteriorParticles = sourceParticlesData.domainParticles;
        auto const sourceGhostParticles     = sourceParticlesData.ghostParticles;

        auto& destinationCoarseBoundaryParticles = destinationParticlesData.coarseToFineParticles;
        auto& destinationDomainParticles         = destinationParticlesData.domainParticles;

        auto& destinationCoarseBoundaryOldParticles
            = destinationParticlesData.coarseToFineParticlesOld;
        auto& destinationCoarseBoundaryNewParticles
            = destinationParticlesData.coarseToFineParticlesNew;

        auto const& sourceGhostBox       = sourceParticlesData.getGhostBox();
        auto const& destinationGhostBox  = destinationParticlesData.getGhostBox();
        auto const& destinationDomainBox = destinationParticlesData.getBox();

        // For the moment this is just 1D

        // TODO split 1D
        constexpr uint32 refinedParticleNbr{2};
        Split<dim, interpOrder> split{static_cast<uint32>(ratio(dirX)), refinedParticleNbr};

        for (auto const& destinationBox : destinationBoxes)
        {
            auto sourceBox = destinationBox;

            // from each boxes we compute the coarsen one

            sourceBox.coarsen(ratio);

            auto localDestinationBox = destinationBox;


            // after that we get the coarse box, we will need to grow it to get the particles
            // that may after split be inside the destination region

            auto growthVector = SAMRAI::hier::IntVector{SAMRAI::tbox::Dimension{dim},
                                                        ghostWidthForParticles<interpOrder>()};

            sourceBox.grow(growthVector);

            // we then intersect it with the sourceGhostBox , in order to get the local index
            // from the source data

            sourceBox = sourceBox * sourceGhostBox;


            auto localSourceBox = AMRToLocal(
                static_cast<std::add_const_t<decltype(sourceBox)>>(sourceBox), sourceGhostBox);

            localDestinationBox = AMRToLocal(
                static_cast<std::add_const_t<decltype(localDestinationBox)>>(localDestinationBox),
                destinationGhostBox);

            // this is same as isIn but with samrai boxes
            auto isInBox = [](auto const& particle, auto const& localSourceBox) {
                if constexpr (dim == 1)
                {
                    return particle.iCell[dirX] >= localSourceBox.lower(dirX)
                           && particle.iCell[dirX] <= localSourceBox.upper(dirX);
                }
                else if constexpr (dim == 2)
                {
                    auto inDirX = particle.iCell[dirX] >= localSourceBox.lower(dirX)
                                  && particle.iCell[dirX] <= localSourceBox.upper(dirX);
                    auto inDirY = particle.iCell[dirY] >= localSourceBox.lower(dirY)
                                  && particle.iCell[dirY] <= localSourceBox.upper(dirY);

                    return inDirX && inDirY;
                }
                else if constexpr (dim == 3)
                {
                    auto inDirX = particle.iCell[dirX] >= localSourceBox.lower(dirX)
                                  && particle.iCell[dirX] <= localSourceBox.upper(dirX);

                    auto inDirY = particle.iCell[dirY] >= localSourceBox.lower(dirY)
                                  && particle.iCell[dirY] <= localSourceBox.upper(dirY);

                    auto inDirZ = particle.iCell[dirZ] >= localSourceBox.lower(dirZ)
                                  && particle.iCell[dirZ] <= localSourceBox.upper(dirZ);

                    return inDirX && inDirY && inDirZ;
                }
            };

            // this will shift coarse particle as a fine particle : coarse particle position as a
            // refined particles
            auto shiftParticle = [&sourceGhostBox, &destinationGhostBox, &ratio](auto& particle) {
                particle.iCell = localToAMR(particle.iCell, sourceGhostBox);
                particle.iCell = refinedPosition(particle.iCell, ratio);
                particle.iCell = AMRToLocal(particle.iCell, destinationGhostBox);

                // we have an iCell that correspond to the first fine iCell on top of a coarse iCell
                //  since we have refined the iCell position, we have to do the same for the delta
                // it will allow us to get the new iCell and after that to get the correct delta

                auto shiftDeltaAndiCell = [&particle, &ratio](auto direction) {
                    double normalizedPosition
                        = particle.iCell[direction] + ratio(direction) * particle.delta[direction];

                    particle.iCell[direction] = static_cast<int>(normalizedPosition);

                    particle.delta[direction]
                        = normalizedPosition - static_cast<double>(particle.iCell[direction]);
                };

                shiftDeltaAndiCell(dirX);
                if constexpr (dim > 1)
                {
                    shiftDeltaAndiCell(dirY);
                }
                if constexpr (dim > 2)
                {
                    shiftDeltaAndiCell(dirZ);
                }
            };


            // Now we need to get the physical position of the boundary of the destinationBox.
            // for that we need to consider the domainBox of the destinationPatchData as the
            // reference Box

            auto* dx     = pGeom.getDx();
            auto* xLower = pGeom.getXLower();

            Point<double, dim> physicalLowerDestination;
            Point<double, dim> physicalUpperDestination;

            auto destinationBoxLocalToDomain = AMRToLocal(
                static_cast<std::add_const_t<decltype(destinationBox)>>(destinationBox),
                destinationDomainBox);

            // TODO this formula is duplicated somewhere
            physicalLowerDestination[dirX]
                = xLower[dirX] + dx[dirX] * destinationBoxLocalToDomain.lower(dirX);
            physicalUpperDestination[dirX]
                = xLower[dirX] + dx[dirX] * (destinationBoxLocalToDomain.upper(dirX) + 1);

            if constexpr (dim > 1)
            {
                physicalLowerDestination[dirY]
                    = xLower[dirY] + dx[dirY] * destinationBoxLocalToDomain.lower(dirY);

                physicalUpperDestination[dirY]
                    = xLower[dirY] + dx[dirY] * (destinationBoxLocalToDomain.upper(dirY) + 1);
            }
            if constexpr (dim > 2)
            {
                physicalLowerDestination[dirZ]
                    = xLower[dirZ] + dx[dirZ] * destinationBoxLocalToDomain.lower(dirZ);

                physicalUpperDestination[dirZ]
                    = xLower[dirZ] + dx[dirZ] * (destinationBoxLocalToDomain.upper(dirZ) + 1);
            }

            [[maybe_unused]] auto isCandidateForSplit
                = [&physicalLowerDestination, &physicalUpperDestination, dx,
                   xLower](auto const& particle) {
                      if constexpr (dim == 1)
                      {
                          double maxDistanceX = 0;

                          if constexpr (interpOrder == 1)
                          {
                              maxDistanceX = 0.5 * dx[dirX];
                          }
                          else if constexpr (interpOrder == 2)
                          {
                              maxDistanceX = (1.5 / 2) * dx[dirX];
                          }
                          else if constexpr (interpOrder == 3)
                          {
                              maxDistanceX = (3 / 2) * dx[dirX];
                          }
                          static_assert(interpOrder >= 1 && interpOrder <= 3);


                          double particlesPositionX = xLower[dirX] + particle.iCell[dirX] * dx[dirX]
                                                      + particle.delta[dirX] * dx[dirX];
                          double distanceFromLowerX
                              = std::abs(particlesPositionX - physicalLowerDestination[dirX]);
                          double distanceFromUpperX
                              = std::abs(particlesPositionX - physicalUpperDestination[dirX]);

                          return (distanceFromLowerX <= maxDistanceX
                                  || distanceFromUpperX <= maxDistanceX);
                      }
                  };

            auto isInSplit = [&physicalLowerDestination, &physicalUpperDestination, dx,
                              xLower](auto const& particle) {
                if constexpr (dim == 1)
                {
                    double particlesPositionX = xLower[dirX] + particle.iCell[dirX] * dx[dirX]
                                                + particle.delta[dirX] * dx[dirX];

                    return particlesPositionX >= physicalLowerDestination[dirX]
                           && particlesPositionX <= physicalUpperDestination[dirX];
                }
            };

            // Since we are in a temporary space, we may have to copy information
            // from ghost region as well. This operator will perform the split
            // on particles in domain and ghost zone, and put the split particles
            // in the coarseToFineParticles.
            std::array<std::remove_reference_t<decltype(sourceInteriorParticles)>*, 2>
                particlesArrays{{&sourceInteriorParticles, &sourceGhostParticles}};




            for (auto const& sourceParticlesArray : particlesArrays)
            {
                for (auto const& particle : *sourceParticlesArray)
                {
                    //

                    if (isInBox(particle, localSourceBox))
                    {
                        //
                        auto shiftedParticle = particle;
                        shiftParticle(shiftedParticle);

                        bool constexpr isCoarseBoundarySplitType
                            = splitType == ParticlesDataSplitType::coarseBoundary
                              || splitType == ParticlesDataSplitType::coarseBoundary1
                              || splitType == ParticlesDataSplitType::coarseBoundary2;

                        if constexpr (isCoarseBoundarySplitType)
                        {
                            if (isCandidateForSplit(shiftedParticle))
                            {
                                std::vector<Particle<dim>> splittedParticles;

                                split(shiftedParticle, splittedParticles);

                                for (auto const& splittedParticle : splittedParticles)
                                {
                                    if (isInSplit(splittedParticle))
                                    {
                                        if constexpr (splitType
                                                      == ParticlesDataSplitType::coarseBoundary)
                                        {
                                            destinationCoarseBoundaryParticles.push_back(
                                                splittedParticle);
                                        }
                                        else if constexpr (splitType
                                                           == ParticlesDataSplitType::
                                                                  coarseBoundary1)
                                        {
                                            //
                                            destinationCoarseBoundaryOldParticles.push_back(
                                                splittedParticle);
                                        }
                                        else if constexpr (splitType
                                                           == ParticlesDataSplitType::
                                                                  coarseBoundary2)
                                        {
                                            //
                                            destinationCoarseBoundaryNewParticles.push_back(
                                                splittedParticle);
                                        }
                                    }
                                }
                            }
                        }
                        else if constexpr (splitType == ParticlesDataSplitType::interior)
                        {
                            std::vector<Particle<dim>> splittedParticles;
                            split(shiftedParticle, splittedParticles);
                            for (auto const& splittedParticle : splittedParticles)
                            {
                                if (isInSplit(splittedParticle))
                                {
                                    destinationDomainParticles.push_back(splittedParticle);
                                }
                            }
                        }
                    } // is in source selected box
                }     // loop on particle
            }         // loop on particelesArrays
        }             // loop on destination box
    }
};


} // namespace PHARE

#endif
