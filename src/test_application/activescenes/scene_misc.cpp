/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "scene_misc.h"
#include "scene_physics.h"
#include "scenarios.h"
#include "identifiers.h"

#include "CameraController.h"

#include <osp/Active/basic.h>
#include <osp/Active/drawing.h>

#include <osp/Active/SysSceneGraph.h>
#include <osp/Active/SysRender.h>

#include <osp/Resource/resources.h>

#include <osp/unpack.h>

using namespace osp;
using namespace osp::active;

using osp::phys::EShape;
using osp::input::EButtonControlIndex;

// for the 0xrrggbb_rgbf and angle literals
using namespace Magnum::Math::Literals;

namespace testapp::scenes
{

void create_materials(
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        int const                   count)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene, TESTAPP_DATA_COMMON_SCENE);
    auto &rDrawing = top_get< ACtxDrawing >(topData, idDrawing);

    for (int i = 0; i < count; ++i)
    {
        [[maybe_unused]] MaterialId const mat = rDrawing.m_materialIds.create();
        LGRN_ASSERT(int(mat) == i);
    }

    rDrawing.m_materials.resize(count);
}

void add_floor(
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              commonScene,
        Session const&              shapeSpawn,
        MaterialId const            materialId,
        PkgId const                 pkg)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,    TESTAPP_DATA_SHAPE_SPAWN);

    auto &rResources    = top_get< Resources >          (topData, idResources);
    auto &rBasic        = top_get< ACtxBasic >          (topData, idBasic);
    auto &rDrawing      = top_get< ACtxDrawing >        (topData, idDrawing);
    auto &rDrawingRes   = top_get< ACtxDrawingRes >     (topData, idDrawingRes);
    auto &rSpawner      = top_get< ACtxShapeSpawner >   (topData, idSpawner);

    Material &rMaterial = rDrawing.m_materials.at(materialId);

    // Convenient functor to get a reference-counted mesh owner
    auto const quick_add_mesh = SysRender::gen_drawable_mesh_adder(rDrawing, rDrawingRes, rResources, pkg);

    // start making floor

    static constexpr Vector3 const sc_floorSize{64.0f, 64.0f, 1.0f};
    static constexpr Vector3 const sc_floorPos{0.0f, 0.0f, -1.005f};

    // Create floor root and mesh entity
    ActiveEnt const floorRootEnt = rBasic.m_activeIds.create();
    ActiveEnt const floorMeshEnt = rBasic.m_activeIds.create();
    DrawEnt const floorMeshDrawEnt = rDrawing.m_drawIds.create();

    // Resize some containers to fit all existing entities
    rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());
    rDrawing.resize_active(rBasic.m_activeIds.capacity());
    rDrawing.resize_draw();
    bitvector_resize(rMaterial.m_ents, rDrawing.m_drawIds.capacity());

    rBasic.m_transform.emplace(floorRootEnt);

    // Add mesh to floor mesh entity
    rDrawing.m_activeToDraw[floorMeshEnt] = floorMeshDrawEnt;
    rDrawing.m_mesh[floorMeshDrawEnt] = quick_add_mesh("grid64solid");
    rDrawing.m_meshDirty.push_back(floorMeshDrawEnt);

    // Add mesh visualizer material to floor mesh entity
    rMaterial.m_ents.set(std::size_t(floorMeshDrawEnt));
    rMaterial.m_dirty.push_back(floorMeshDrawEnt);

    // Add transform, draw transform, opaque, and visible entity
    rBasic.m_transform.emplace(floorMeshEnt, ACompTransform{Matrix4::scaling(sc_floorSize)});
    rDrawing.m_drawBasic[floorMeshDrawEnt].m_opaque = true;
    rDrawing.m_visible.set(std::size_t(floorMeshDrawEnt));
    rDrawing.m_needDrawTf.set(std::size_t(floorRootEnt));
    rDrawing.m_needDrawTf.set(std::size_t(floorMeshEnt));

    SubtreeBuilder builder = SysSceneGraph::add_descendants(rBasic.m_scnGraph, 2);

    // Add floor root to hierarchy root
    SubtreeBuilder bldFloorRoot = builder.add_child(floorRootEnt, 1);

    // Parent floor mesh entity to floor root entity
    bldFloorRoot.add_child(floorMeshEnt);

    // Add collider to floor root entity
    rSpawner.m_spawnRequest.emplace_back(SpawnShape{
        .m_position = sc_floorPos,
        .m_velocity = sc_floorSize,
        .m_size     = sc_floorSize,
        .m_mass     = 0.0f,
        .m_shape    = EShape::Box
    });
}

Session setup_camera_ctrl(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scnRender)
{
    OSP_DECLARE_GET_DATA_IDS(windowApp,     TESTAPP_DATA_WINDOW_APP);
    OSP_DECLARE_GET_DATA_IDS(scnRender,     TESTAPP_DATA_COMMON_RENDERER);

    auto const tgSR  = scnRender.get_pipelines<PlSceneRenderer>();
    //auto const tgWin = windowApp.get_pipelines<PlWindowApp>();

    auto &rUserInput = top_get< osp::input::UserInputHandler >(topData, idUserInput);

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_CAMERA_CTRL);
    auto const tgCmCt = out.create_pipelines<PlCameraCtrl>(rBuilder);

    top_emplace< ACtxCameraController > (topData, idCamCtrl, rUserInput);

    rBuilder.pipeline(tgCmCt.camCtrl).parent(tgSR.render);

    rBuilder.task()
        .name       ("Position Rendering Camera according to Camera Controller")
        .run_on     ({tgSR.render(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgSR.camera(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                           idCamCtrl,        idCamera })
        .func([] (ACtxCameraController const& rCamCtrl, Camera &rCamera) noexcept
    {
        rCamera.m_transform = rCamCtrl.m_transform;
    });

    return out;
}

Session setup_camera_free(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              scene,
        Session const&              cameraCtrl)
{
    OSP_DECLARE_GET_DATA_IDS(scene,         TESTAPP_DATA_SCENE);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,    TESTAPP_DATA_CAMERA_CTRL);

    auto const tgWin    = windowApp     .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl    .get_pipelines<PlCameraCtrl>();

    Session out;

    rBuilder.task()
        .name       ("Move Camera controller")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Modify)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,           idDeltaTimeIn })
        .func([] (ACtxCameraController& rCamCtrl, float const deltaTimeIn) noexcept
    {
        SysCameraController::update_view(rCamCtrl, deltaTimeIn);
        SysCameraController::update_move(rCamCtrl, deltaTimeIn, true);
    });

    return out;
}


Session setup_thrower(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              cameraCtrl,
        Session const&              shapeSpawn)
{
    OSP_DECLARE_GET_DATA_IDS(shapeSpawn,     TESTAPP_DATA_SHAPE_SPAWN);
    OSP_DECLARE_GET_DATA_IDS(cameraCtrl,   TESTAPP_DATA_CAMERA_CTRL);
    auto &rCamCtrl = top_get< ACtxCameraController > (topData, idCamCtrl);

    auto const tgWin    = windowApp .get_pipelines<PlWindowApp>();
    auto const tgCmCt   = cameraCtrl.get_pipelines<PlCameraCtrl>();
    auto const tgShSp   = shapeSpawn.get_pipelines<PlShapeSpawn>();

    Session out;
    auto const [idBtnThrow] = out.acquire_data<1>(topData);

    top_emplace< EButtonControlIndex > (topData, idBtnThrow, rCamCtrl.m_controls.button_subscribe("debug_throw"));

    rBuilder.task()
        .name       ("Throw spheres when pressing space")
        .run_on     ({tgWin.inputs(Run)})
        .sync_with  ({tgCmCt.camCtrl(Ready), tgShSp.spawnRequest(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({                 idCamCtrl,                  idSpawner,                   idBtnThrow })
        .func([] (ACtxCameraController& rCamCtrl, ACtxShapeSpawner& rSpawner, EButtonControlIndex btnThrow) noexcept
    {
        // Throw a sphere when the throw button is pressed
        if (rCamCtrl.m_controls.button_held(btnThrow))
        {
            Matrix4 const &camTf = rCamCtrl.m_transform;
            float const speed = 120;
            float const dist = 8.0f;
            rSpawner.m_spawnRequest.emplace_back(SpawnShape{
                .m_position = camTf.translation() - camTf.backward() * dist,
                .m_velocity = -camTf.backward() * speed,
                .m_size     = Vector3{1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Sphere
            });
        }
    });

    return out;
}

/*

Session setup_droppers(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              commonScene,
        Session const&              shapeSpawn)
{
    OSP_SESSION_UNPACK_DATA(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_TAGS(commonScene,  TESTAPP_COMMON_SCENE);
    OSP_SESSION_UNPACK_DATA(shapeSpawn, TESTAPP_SHAPE_SPAWN);
    OSP_SESSION_UNPACK_TAGS(shapeSpawn, TESTAPP_SHAPE_SPAWN);

    Session droppers;
    auto const [idSpawnTimerA, idSpawnTimerB] = droppers.acquire_data<2>(topData);

    top_emplace< float > (topData, idSpawnTimerA, 0.0f);
    top_emplace< float > (topData, idSpawnTimerB, 0.0f);

    droppers.task() = rBuilder.task().assign({tgTimeEvt, tgSpawnMod}).data(
            "Spawn blocks every 2 seconds",
            TopDataIds_t{              idSpawner,       idSpawnTimerA,          idDeltaTimeIn },
            wrap_args([] (SpawnerVec_t& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 2.0f)
        {
            rSpawnTimer -= 2.0f;

            rSpawner.emplace_back(SpawnShape{
                .m_position = {10.0f, 0.0f, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Box
            });
        }
    }));

    droppers.task() = rBuilder.task().assign({tgTimeEvt, tgSpawnMod}).data(
            "Spawn cylinders every 1 seconds",
            TopDataIds_t{              idSpawner,       idSpawnTimerB,          idDeltaTimeIn },
            wrap_args([] (SpawnerVec_t& rSpawner, float& rSpawnTimer, float const deltaTimeIn) noexcept
    {
        rSpawnTimer += deltaTimeIn;
        if (rSpawnTimer >= 1.0f)
        {
            rSpawnTimer -= 1.0f;

            rSpawner.emplace_back(SpawnShape{
                .m_position = {-10.0f, 0.0, 30.0f},
                .m_velocity = {0.0f, 0.0f, 0.0f},
                .m_size     = {2.0f, 2.0f, 1.0f},
                .m_mass     = 1.0f,
                .m_shape    = EShape::Cylinder
            });
        }
    }));

    return droppers;
}*/

} // namespace testapp::scenes
