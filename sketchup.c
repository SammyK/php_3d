#include "sketchup.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <SketchUpAPI/common.h>
#include <SketchUpAPI/initialize.h>
#include <SketchUpAPI/geometry.h>
#include <SketchUpAPI/geometry/transformation.h>
#include <SketchUpAPI/model/model.h>
#include <SketchUpAPI/model/component_definition.h>
#include <SketchUpAPI/model/component_instance.h>
#include <SketchUpAPI/model/entities.h>
#include <SketchUpAPI/model/face.h>
#include <SketchUpAPI/model/edge.h>
#include <SketchUpAPI/model/layer.h>
#include <SketchUpAPI/model/loop.h>
#include <SketchUpAPI/model/group.h>
#include <SketchUpAPI/model/geometry_input.h>
#include <SketchUpAPI/model/material.h>
#include <SketchUpAPI/model/vertex.h>

#define SU_CALL_RETURN(func) { \
	enum SUResult res__ = func; \
	if (res__ != SU_ERROR_NONE) { \
		assert(res__ == SU_ERROR_NONE); \
		return false; \
	} \
}

static bool sup_copy_geometry(SUEntitiesRef dest, SUEntitiesRef src) {
	SUGeometryInputRef geom_input = SU_INVALID;
	// TODO Free this when anything fails below
	SU_CALL_RETURN(SUGeometryInputCreate(&geom_input));

	size_t vertices_count = 0;
	size_t face_count = 0;
	SU_CALL_RETURN(SUEntitiesGetNumFaces(src, &face_count));
	printf("Found %zu source faces\n", face_count);
	if (face_count > 0) {
		SUFaceRef faces[face_count];
		SU_CALL_RETURN(SUEntitiesGetFaces(src, face_count, &faces[0], &face_count));
		// We cannot simply do the following here:
		// SUEntitiesAddFaces(dest_entities, face_count, &faces[0]);
		// This will share memory between models and when the first model is freed, it will cause a crash when the second model is freed
		// Therefore we must maticulously copy every property one by one.
		for (size_t i = 0; i < face_count; i++) {
			size_t vertex_count = 0;
			SU_CALL_RETURN(SUFaceGetNumVertices(faces[i], &vertex_count));
			printf("\tCopying %zu source face #%zu vertices...", vertex_count, i);
			if (vertex_count > 0) {
				SUVertexRef vertices[vertex_count];
				SU_CALL_RETURN(SUFaceGetVertices(faces[i], vertex_count, vertices, &vertex_count));

				SULoopInputRef loop = SU_INVALID;
				// TODO Free this if anything fails below
				SU_CALL_RETURN(SULoopInputCreate(&loop));
				for (size_t j = 0; j < vertex_count; ++j) {
					struct SUPoint3D pos = {0.0};
					SU_CALL_RETURN(SUVertexGetPosition(vertices[j], &pos));
					// This seems to auto increment zero-based indexes
					SU_CALL_RETURN(SUGeometryInputAddVertex(geom_input, &pos));
					// Indexes are global to geom_input so we cannot use j for index
					SU_CALL_RETURN(SULoopInputAddVertexIndex(loop, vertices_count++));
					printf("#%zu, ", vertices_count - 1);
				}
				printf("\n");

				size_t face_index = 0;
				SU_CALL_RETURN(SUGeometryInputAddFace(geom_input, &loop, &face_index));
				printf("\t\tCopied face #%zu to index #%zu\n", i, face_index);

				// Copy inner loops
				size_t inner_loops_count = 0;
				SU_CALL_RETURN(SUFaceGetNumInnerLoops(faces[i], &inner_loops_count));
				if (inner_loops_count > 0) {
					printf("\t\tSource face #%zu has %zu inner loops.\n", i, inner_loops_count);
					SULoopRef inner_loops[inner_loops_count];
					SU_CALL_RETURN(SUFaceGetInnerLoops(faces[i], inner_loops_count, inner_loops, &inner_loops_count));
					for (size_t y = 0; y < inner_loops_count; ++y) {
						size_t loop_vertex_count = 0;
						SU_CALL_RETURN(SULoopGetNumVertices(inner_loops[y], &loop_vertex_count));
						if (loop_vertex_count > 0) {
							printf("\t\t\tInner loop #%zu has %zu vertices.\n", y, loop_vertex_count);
							SULoopInputRef inner_loop = SU_INVALID;
							// TODO Free this if anything fails below
							SU_CALL_RETURN(SULoopInputCreate(&inner_loop));
							SUVertexRef inner_loop_vertices[loop_vertex_count];
							SU_CALL_RETURN(SULoopGetVertices(inner_loops[y], loop_vertex_count, inner_loop_vertices, &loop_vertex_count));
							for (size_t x = 0; x < loop_vertex_count; ++x) {
								struct SUPoint3D pos = {0.0};
								SU_CALL_RETURN(SUVertexGetPosition(inner_loop_vertices[x], &pos));
								// This seems to auto increment zero-based indexes
								SU_CALL_RETURN(SUGeometryInputAddVertex(geom_input, &pos));
								// Indexes are global to geom_input so we cannot use x for index
								SU_CALL_RETURN(SULoopInputAddVertexIndex(inner_loop, vertices_count++));
								printf("\t\t\tCopied inner vertex #%zu at {%f, %f, %f} to index #%zu\n", x, pos.x, pos.y, pos.z, vertices_count - 1);
							}
							SU_CALL_RETURN(SUGeometryInputFaceAddInnerLoop(geom_input, face_index, &inner_loop));
						}
					}
				}

				// Copy front and back materials
				printf("\t\tCopying materials to face at index #%zu\n", face_index);
				struct SUMaterialInput material_input = {0};
				SUMaterialRef material = SU_INVALID;
				enum SUResult res = SUFaceGetFrontMaterial(faces[i], &material);
				if (res == SU_ERROR_NO_DATA || res == SU_ERROR_NULL_POINTER_OUTPUT) {
					SUMaterialRef default_front_material = SU_INVALID;
					SU_CALL_RETURN(SUMaterialCreate(&default_front_material));
					SUColor default_front_color = {0};
					SU_CALL_RETURN(SUColorSetByValue(&default_front_color, 0xffffff));
					SU_CALL_RETURN(SUMaterialSetColor(default_front_material, &default_front_color));
					material_input.material = default_front_material;
					SU_CALL_RETURN(SUGeometryInputFaceSetFrontMaterial(geom_input, face_index, &material_input));
				} else {
					assert(res == SU_ERROR_NONE);
					// TODO Copy other material attributes
					material_input.material = material;
					// See also SUGeometryInputFaceSetFrontMaterialByPosition
					SU_CALL_RETURN(SUGeometryInputFaceSetFrontMaterial(geom_input, face_index, &material_input));
				}
				res = SUFaceGetBackMaterial(faces[i], &material);
				if (res == SU_ERROR_NO_DATA || res == SU_ERROR_NULL_POINTER_OUTPUT) {
					// TODO Default the material?
				} else {
					assert(res == SU_ERROR_NONE);
					// TODO Copy other material attributes
					material_input.material = material;
					// See also SUGeometryInputFaceSetBackMaterialByPosition
					SU_CALL_RETURN(SUGeometryInputFaceSetBackMaterial(geom_input, face_index, &material_input));
				}
			}
		}
	}

	enum SUResult res = SUEntitiesFill(dest, geom_input, true);
	SUGeometryInputRelease(&geom_input);

	if (res != SU_ERROR_NONE) {
		printf("Failed to fill dest geometry\n");
		return false;
	}
	printf("Successfully filled dest geometry\n");
	return true;
}

static bool sup_component_def_load(SUModelRef model, const char *name, const char *file, SUComponentDefinitionRef *def) {
	// Init empty component def, attach to model, and get entities
	// TODO Free component def if stuff fails below
	SU_CALL_RETURN(SUComponentDefinitionCreate(def));
	SUComponentDefinitionRef tmp[] = {*def};
	if ((SUModelAddComponentDefinitions(model, 1, tmp)) != SU_ERROR_NONE) {
		SU_CALL_RETURN(SUComponentDefinitionRelease(def));
		return false;
	}
	SU_CALL_RETURN(SUComponentDefinitionSetName(*def, name));

	// Load model from file and get source entities
	SUModelRef src_model = SU_INVALID;
	enum SUModelLoadStatus status;
	SU_CALL_RETURN(SUModelCreateFromFileWithStatus(&src_model, file, &status));
	assert(status != SUModelLoadStatus_Success_MoreRecent && "Update SDK version to load this file.");
	SUEntitiesRef src_entities = SU_INVALID;
	SU_CALL_RETURN(SUModelGetEntities(src_model, &src_entities));

	// Get dest entities from component def
	SUEntitiesRef dest_entities = SU_INVALID;
	SU_CALL_RETURN(SUComponentDefinitionGetEntities(*def, &dest_entities));

	// Copy all file entities to component def entities
	return sup_copy_geometry(dest_entities, src_entities);
}

static bool sup_component_def_create_instance(SUModelRef model, SUComponentDefinitionRef def, const char *name, SUComponentInstanceRef *instance) {
	// Create component instance from definition and attach to model entities
	// TODO Free the instance if anything fails below
	SU_CALL_RETURN(SUComponentDefinitionCreateInstance(def, instance));
	// TODO Convert component instace to group? SUGroupFromComponentInstance()
	SUEntitiesRef entities = SU_INVALID;
	SU_CALL_RETURN(SUModelGetEntities(model, &entities));
	SUStringRef name_utf8 = SU_INVALID;
	SU_CALL_RETURN(SUStringCreateFromUTF8(&name_utf8, name));
	SU_CALL_RETURN(SUEntitiesAddInstance(entities, *instance, &name_utf8));
	SU_CALL_RETURN(SUComponentInstanceSetName(*instance, name));
	return true;
}

// TODO Refactor this to work with instances of variables too
static bool sup_component_instance_move(SUComponentInstanceRef instance, double offset) {
	if (offset <= 0.0) return true;
	SUComponentDefinitionRef def = SU_INVALID;
	SU_CALL_RETURN(SUComponentInstanceGetDefinition(instance, &def));

	SUEntitiesRef entities = SU_INVALID;
	SU_CALL_RETURN(SUComponentDefinitionGetEntities(def, &entities));
	struct SUBoundingBox3D bbox;
	SU_CALL_RETURN(SUEntitiesGetBoundingBox(entities, &bbox));

	// We assume all components on y axis have the same y length
	struct SUTransformation transform = {0.0};
	SU_CALL_RETURN(SUComponentInstanceGetTransform(instance, &transform));
	struct SUVector3D point = {0.0, (bbox.max_point.y * offset) /* Follow the white rabbit */, 0.0};
	SU_CALL_RETURN(SUTransformationTranslation(&transform, &point));
	SU_CALL_RETURN(SUComponentInstanceSetTransform(instance, &transform));
	return true;
}

void sketchup_startup(void) { SUInitialize(); }
void sketchup_shutdown(void) { SUTerminate(); }

#define SUP_MAX_ROOMS 256

// Ideally we'd track component defs on the model, but there seems to be a bug in the SketchUpAPI with getting the def name, description and Guid
typedef struct sup_building_impl_s {
	SUModelRef model;
	SUComponentDefinitionRef room_def;
	SUComponentInstanceRef rooms[SUP_MAX_ROOMS];
} sup_building_impl;

bool sketchup_building_ctor(sketchup_building *building) {
	// Create new model
	SUModelRef model = SU_INVALID;
	enum SUResult res = SUModelCreate(&model);
	if (res != SU_ERROR_NONE) return false;

	sup_building_impl *building_impl = (sup_building_impl *)calloc(1, sizeof(sup_building_impl));
	building_impl->model = model;

	// Create component defs from files and save to model
	if (!sup_component_def_load(model, "room", "models/room.skp", &building_impl->room_def)) {
		SUModelRelease(&model);
		free(building_impl);
		return false;
	}
	building->ptr = building_impl;
	return true;
}

bool sketchup_building_append_room(sketchup_building building, const char *name, size_t room_index) {
	if (room_index >= SUP_MAX_ROOMS) return false;
	sup_building_impl *building_impl = (sup_building_impl *)building.ptr;
	SUComponentInstanceRef *room = &building_impl->rooms[room_index];
	if (room->ptr) return false;
	if (!sup_component_def_create_instance(building_impl->model, building_impl->room_def, name, room)) return false;
	// Move instance to proper offset on y axis
	if (!sup_component_instance_move(*room, (double) room_index)) return false;
	return true;
}

bool sketchup_building_save(sketchup_building building, const char *file) {
	sup_building_impl *building_impl = (sup_building_impl *)building.ptr;
	enum SUResult res = SUModelSaveToFileWithVersion(building_impl->model, file, SUModelVersion_SU2021);
	return (res == SU_ERROR_NONE);
}

bool sketchup_building_dtor(sketchup_building building) {
	sup_building_impl *building_impl = (sup_building_impl *)building.ptr;
	enum SUResult res = SUModelRelease(&building_impl->model);
	free(building_impl);
	return (res == SU_ERROR_NONE);
}

bool sketchup_room_append_variable(sketchup_building building, size_t room_index, sketchup_val val) {
	if (room_index >= SUP_MAX_ROOMS) return false;
	sup_building_impl *building_impl = (sup_building_impl *)building.ptr;
	SUComponentInstanceRef room = building_impl->rooms[room_index];
	return true;
}

void sketchup_sdk_version(size_t bufsiz, char *version) {
	size_t major = 0;
	size_t minor = 0;
	SUGetAPIVersion(&major, &minor);
	snprintf(version, bufsiz, "%zu.%zu", major, minor);
}
