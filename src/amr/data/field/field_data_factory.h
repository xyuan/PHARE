#ifndef PHARE_SRC_AMR_FIELD_FIELD_DATA_FACTORY_H
#define PHARE_SRC_AMR_FIELD_FIELD_DATA_FACTORY_H

#include <SAMRAI/geom/CartesianPatchGeometry.h>
#include <SAMRAI/hier/Patch.h>
#include <SAMRAI/hier/PatchDataFactory.h>
#include <SAMRAI/tbox/MemoryUtilities.h>

#include <utility>

#include "data/grid/gridlayout.h"
#include "data/grid/gridlayout_impl.h"
#include "field_data.h"

namespace PHARE
{
template<typename GridLayoutT, typename FieldImpl,
         typename PhysicalQuantity = decltype(std::declval<FieldImpl>().physicalQuantity())>
/**
 * @brief The FieldDataFactory class
 */
class FieldDataFactory : public SAMRAI::hier::PatchDataFactory
{
public:
    static constexpr std::size_t dimension    = GridLayoutT::dimension;
    static constexpr std::size_t interp_order = GridLayoutT::interp_order;


    FieldDataFactory(bool fineBoundaryRepresentsVariable, bool dataLivesOnPatchBorder,
                     std::string const& name, PhysicalQuantity qty)
        : SAMRAI::hier::PatchDataFactory(
              SAMRAI::hier::IntVector{SAMRAI::tbox::Dimension(dimension), 5})
        , fineBoundaryRepresentsVariable_{fineBoundaryRepresentsVariable}
        , dataLivesOnPatchBorder_{dataLivesOnPatchBorder}
        , quantity_{qty}
        , name_{name}
    {
    }




    /*** \brief Clone the current FieldDataFactory
     */
    std::shared_ptr<SAMRAI::hier::PatchDataFactory>
    cloneFactory(SAMRAI::hier::IntVector const& ghost) final
    {
        (void)ghost;
        return std::make_shared<FieldDataFactory>(fineBoundaryRepresentsVariable_,
                                                  dataLivesOnPatchBorder_, name_, quantity_);
    }




    /*** \brief Given a patch, allocate a FieldData
     * it is expected that this routines will create a functional fieldData
     * (ie with a gridlayout and a FieldImpl)
     */
    std ::shared_ptr<SAMRAI::hier::PatchData> allocate(SAMRAI::hier::Patch const& patch) const final
    {
        auto const& domain = patch.getBox();
        SAMRAI::tbox::Dimension dim{dimension};



        // We finally make the FieldData with the correct parameter

        return std::make_shared<FieldData<GridLayoutT, FieldImpl>>(
            domain, SAMRAI::hier::IntVector{dim, 5}, name_, layoutFromPatch<GridLayoutT>(patch),
            quantity_);
    }




    std::shared_ptr<SAMRAI::hier::BoxGeometry>
    getBoxGeometry(SAMRAI::hier::Box const& box) const final
    {
        // Note : when we create a FieldGeometry, we don't need to have the correct
        // dxdydz, nor the physical origin. All we have to know is the numberCells
        // for the gridlayout, and also we give the box to the FieldGeometry, so that
        // it can use it to get the final box representation.

        std::array<double, dimension> dl;
        std::array<uint32, dimension> nbCell;
        Point<double, dimension> origin;

        for (std::size_t iDim = 0; iDim < dimension; ++iDim)
        {
            dl[iDim]     = 0.01;
            nbCell[iDim] = box.numberCells(iDim);
            origin[iDim] = 0;
        }


        // dumb dl and origin, only nbCell is usefull
        // but FieldGeometry needs to use a gridlayout instance with proper nbrCells
        GridLayoutT gridLayout(dl, nbCell, origin);

        return std::make_shared<FieldGeometry<GridLayoutT, PhysicalQuantity>>(
            box, std::move(gridLayout), quantity_);
    }




    std::size_t getSizeOfMemory(SAMRAI::hier::Box const& box) const final
    {
        // TODO: this calculus assumes that we don't need more memory than
        //       alignedMemory(nx*ny*nz*sizeof(double)) + alignedMemory(baseSize)

        std::array<double, dimension> dl;
        std::array<uint32, dimension> nbCell;
        Point<double, dimension> origin;

        for (std::size_t iDim = 0; iDim < dimension; ++iDim)
        {
            dl[iDim]     = 0.01; // some value that is not used anyway
            origin[iDim] = 0;
            nbCell[iDim] = box.numberCells(iDim);
        }

        const std::size_t baseField
            = SAMRAI::tbox::MemoryUtilities::align(sizeof(FieldData<GridLayoutT, FieldImpl>));

        GridLayoutT gridLayout{dl, nbCell, origin};


        auto const& allocSize = gridLayout.allocSize(quantity_);

        std::size_t data = 1;
        for (auto nCell : allocSize)
        {
            data *= nCell;
        }

        data *= sizeof(typename FieldImpl::type);



        return baseField + SAMRAI::tbox::MemoryUtilities::align(data);
    }




    bool fineBoundaryRepresentsVariable() const final { return fineBoundaryRepresentsVariable_; }




    bool dataLivesOnPatchBorder() const final { return dataLivesOnPatchBorder_; }



    bool validCopyTo(std::shared_ptr<SAMRAI::hier::PatchDataFactory> const&
                         destinationPatchDataFactory) const final
    {
        auto fieldDataFactory
            = std::dynamic_pointer_cast<FieldDataFactory>(destinationPatchDataFactory);
        return (fieldDataFactory != nullptr);
    }



private:
    bool const fineBoundaryRepresentsVariable_;
    bool const dataLivesOnPatchBorder_;
    PhysicalQuantity const quantity_;
    std::string name_;
};




} // namespace PHARE


#endif
