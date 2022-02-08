#include "sketchup.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include <SketchUpAPI/common.h>
#include <SketchUpAPI/initialize.h>
#include <SketchUpAPI/geometry.h>
#include <SketchUpAPI/geometry/transformation.h>
#include <SketchUpAPI/model/model.h>
#include <SketchUpAPI/model/camera.h>
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
#include <SketchUpAPI/model/scene.h>
#include <SketchUpAPI/model/styles.h>
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
	//printf("Found %zu source faces\n", face_count);
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
			//printf("\tCopying %zu source face #%zu vertices...", vertex_count, i);
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
					//printf("#%zu, ", vertices_count - 1);
				}
				//printf("\n");

				size_t face_index = 0;
				SU_CALL_RETURN(SUGeometryInputAddFace(geom_input, &loop, &face_index));
				//printf("\t\tCopied face #%zu to index #%zu\n", i, face_index);

				// Copy inner loops
				size_t inner_loops_count = 0;
				SU_CALL_RETURN(SUFaceGetNumInnerLoops(faces[i], &inner_loops_count));
				if (inner_loops_count > 0) {
					//printf("\t\tSource face #%zu has %zu inner loops.\n", i, inner_loops_count);
					SULoopRef inner_loops[inner_loops_count];
					SU_CALL_RETURN(SUFaceGetInnerLoops(faces[i], inner_loops_count, inner_loops, &inner_loops_count));
					for (size_t y = 0; y < inner_loops_count; ++y) {
						size_t loop_vertex_count = 0;
						SU_CALL_RETURN(SULoopGetNumVertices(inner_loops[y], &loop_vertex_count));
						if (loop_vertex_count > 0) {
							//printf("\t\t\tInner loop #%zu has %zu vertices.\n", y, loop_vertex_count);
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
								//printf("\t\t\tCopied inner vertex #%zu at {%f, %f, %f} to index #%zu\n", x, pos.x, pos.y, pos.z, vertices_count - 1);
							}
							SU_CALL_RETURN(SUGeometryInputFaceAddInnerLoop(geom_input, face_index, &inner_loop));
						}
					}
				}

				// Copy front and back materials
				//printf("\t\tCopying materials to face at index #%zu\n", face_index);
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
		//printf("Failed to fill dest geometry\n");
		return false;
	}
	//printf("Successfully filled dest geometry\n");
	return true;
}

static bool sup_component_def_load(SUModelRef model, const char *file, SUComponentDefinitionRef *def, struct SUBoundingBox3D *bbox) {
	//printf("Creating component definition from '%s'...\n", file);
	// Init empty component def, attach to model, and get entities
	// TODO Free component def if stuff fails below
	SU_CALL_RETURN(SUComponentDefinitionCreate(def));
	SUComponentDefinitionRef tmp[] = {*def};
	if ((SUModelAddComponentDefinitions(model, 1, tmp)) != SU_ERROR_NONE) {
		SU_CALL_RETURN(SUComponentDefinitionRelease(def));
		return false;
	}
	SU_CALL_RETURN(SUComponentDefinitionSetName(*def, file));

	// Load model from file and get source entities
	SUModelRef src_model = SU_INVALID;
	enum SUModelLoadStatus status;
	SU_CALL_RETURN(SUModelCreateFromFileWithStatus(&src_model, file, &status));
	assert(status != SUModelLoadStatus_Success_MoreRecent && "Update SDK version to load this file.");
	SUEntitiesRef src_entities = SU_INVALID;
	SU_CALL_RETURN(SUModelGetEntities(src_model, &src_entities));

	// Get the bounding box size
	if (bbox) {
		SU_CALL_RETURN(SUEntitiesGetBoundingBox(src_entities, bbox));
	}

	// Get dest entities from component def
	SUEntitiesRef dest_entities = SU_INVALID;
	SU_CALL_RETURN(SUComponentDefinitionGetEntities(*def, &dest_entities));

	// Copy all file entities to component def entities
	return sup_copy_geometry(dest_entities, src_entities);
}

static bool sup_component_def_create_instance(SUModelRef model, SUComponentDefinitionRef def, SUComponentInstanceRef *instance) {
	// Create component instance from definition and attach to model entities
	// TODO Free the instance if anything fails below
	SU_CALL_RETURN(SUComponentDefinitionCreateInstance(def, instance));
	// TODO Convert component instace to group? SUGroupFromComponentInstance()
	SUEntitiesRef entities = SU_INVALID;
	SU_CALL_RETURN(SUModelGetEntities(model, &entities));
	SU_CALL_RETURN(SUEntitiesAddInstance(entities, *instance, NULL));
	return true;
}

static bool sup_component_instance_move(SUComponentInstanceRef instance, struct SUVector3D point) {
	struct SUTransformation transform = {0.0};
	SU_CALL_RETURN(SUComponentInstanceGetTransform(instance, &transform));
	SU_CALL_RETURN(SUTransformationTranslation(&transform, &point));
	SU_CALL_RETURN(SUComponentInstanceSetTransform(instance, &transform));
	return true;
}

void sketchup_startup(void) { SUInitialize(); }
void sketchup_shutdown(void) { SUTerminate(); }

#define TI(var) ((sup_town_impl *)var.ptr)

typedef struct sup_town_impl_s {
	SUModelRef model;
	SUComponentDefinitionRef town_center_def;
	SUComponentDefinitionRef room_def;
	SUComponentDefinitionRef var_def;
	SUComponentDefinitionRef var_undef_def;
	SUComponentDefinitionRef var_null_def;
	SUComponentDefinitionRef var_false_def;
	SUComponentDefinitionRef var_true_def;
	SUComponentDefinitionRef var_long_def;
	SUComponentDefinitionRef var_double_def;
	SUComponentDefinitionRef var_string_def;
	SUComponentDefinitionRef var_array_def;
	SUComponentDefinitionRef var_object_def;
	SUComponentDefinitionRef var_resource_def;
	SUComponentDefinitionRef var_reference_def;
	struct SUBoundingBox3D town_center_bbox;
	struct SUBoundingBox3D room_bbox;
	struct SUBoundingBox3D var_bbox;
} sup_town_impl;

bool sketchup_town_ctor(sketchup_town *town) {
	// Create a fresh model that we can load all the component defs into
	SUModelRef model = SU_INVALID;
	enum SUResult res = SUModelCreate(&model);
	if (res != SU_ERROR_NONE) return false;

	sup_town_impl *ti = (sup_town_impl *)calloc(1, sizeof(sup_town_impl));
	ti->model = model;

	if (
		!sup_component_def_load(model, "models/town_center.skp", &ti->town_center_def, &ti->town_center_bbox) ||
		!sup_component_def_load(model, "models/room.skp", &ti->room_def, &ti->room_bbox) ||
		!sup_component_def_load(model, "models/var.skp", &ti->var_def, &ti->var_bbox) ||
		/* >> */ !sup_component_def_load(model, "models/var.skp", &ti->var_undef_def, NULL) ||
		!sup_component_def_load(model, "models/var_null.skp", &ti->var_null_def, NULL) ||
		!sup_component_def_load(model, "models/var_false.skp", &ti->var_false_def, NULL) ||
		!sup_component_def_load(model, "models/var_true.skp", &ti->var_true_def, NULL) ||
		!sup_component_def_load(model, "models/var_long.skp", &ti->var_long_def, NULL) ||
		!sup_component_def_load(model, "models/var_double.skp", &ti->var_double_def, NULL) ||
		!sup_component_def_load(model, "models/var_string.skp", &ti->var_string_def, NULL) ||
		!sup_component_def_load(model, "models/var_array.skp", &ti->var_array_def, NULL) ||
		!sup_component_def_load(model, "models/var_object.skp", &ti->var_object_def, NULL) ||
		!sup_component_def_load(model, "models/var_resource.skp", &ti->var_resource_def, NULL) ||
		!sup_component_def_load(model, "models/var_reference.skp", &ti->var_reference_def, NULL)
	) {
		SUModelRelease(&model);
		free(ti);
		return false;
	}

	town->ptr = ti;
	return true;
}

#define BUILDING_PAD 360.0

void sketchup_room_location(sup_town_impl *ti, size_t room_index, size_t visit_index, struct SUVector3D *point) {
	if (room_index == 0) {
		point->x = 0.0;
		point->y = 0.0;
		point->z = 0.0;
		return;
	}

	// There's probably a fancy maths thing to calculate this more elegantly, but I'm no math wiz
	double square = sqrt((double) room_index);
	size_t size = (size_t) floor(square);
	int max = (int) floor((size + 1) / 2);
	int min = max * -1;
	// TODO rename? What even is this?
	int start = min;
	int end = max;
	// Odd
	if ((size % 2) == 1) {
		min++;
		start = max;
		end = min;
	}

	if (round(square) <= size) {
		// All x are same
		point->x = (double) start;
		// This is a special corner so needs special treatment... for some reason
		if (((size * size) + size) == room_index) {
			point->y = point->x;
		} else {
			point->y = (double) end - (room_index % size);
		}
	} else {
		point->x = (double) end - (room_index % size);
		// All y are same
		point->y = (double) start;
	}

	point->x *= (ti->room_bbox.max_point.x + BUILDING_PAD);
	point->y *= (ti->room_bbox.max_point.y + BUILDING_PAD);
	point->z = ti->room_bbox.max_point.z * (double) visit_index;
}

bool sketchup_town_append_room(sketchup_town town, const char *name, size_t room_index, size_t visit_index) {
	if (room_index >= SUP_MAX_ROOMS) return false;
	sup_town_impl *ti = TI(town);
	SUComponentInstanceRef room = SU_INVALID;
	if (!sup_component_def_create_instance(ti->model, (room_index ? ti->room_def : ti->town_center_def), &room)) return false;
	SU_CALL_RETURN(SUComponentInstanceSetName(room, name));

	// TODO Create a SUTextRef to display room name

	// Move room to proper location in the town. We assume all room components are sized the same.
	struct SUVector3D point = {0.0};
	sketchup_room_location(ti, room_index, visit_index, &point);
	if (!sup_component_instance_move(room, point)) return false;
	return true;
}

bool sketchup_town_dtor(sketchup_town town) {
	sup_town_impl *ti = TI(town);
	enum SUResult res = SUModelRelease(&ti->model);
	free(town.ptr);
	return (res == SU_ERROR_NONE);
}

#define WALL_DEPTH 6.0
#define WALL_DEPTH_TC 84.0
#define ROOM_PAD 24.0
#define VAR_PAD 12.0
#define FLOOR_PAD 6.0
#define FLOOR_PAD_TC 30.0

bool sketchup_room_append_variable(sketchup_town town, size_t room_index, size_t visit_index, size_t var_index, const char *name, sketchup_val val) {
	sup_town_impl *ti = TI(town);
	if (room_index >= SUP_MAX_ROOMS) return false;

	SUComponentDefinitionRef def = SU_INVALID;
	switch (val.type) {
		case SKETCHUP_VAL_UNDEF:
			def = ti->var_undef_def;
			break;
		case SKETCHUP_VAL_NULL:
			def = ti->var_null_def;
			break;
		case SKETCHUP_VAL_FALSE:
			def = ti->var_false_def;
			break;
		case SKETCHUP_VAL_TRUE:
			def = ti->var_true_def;
			break;
		case SKETCHUP_VAL_LONG:
			def = ti->var_long_def;
			break;
		case SKETCHUP_VAL_DOUBLE:
			def = ti->var_double_def;
			break;
		case SKETCHUP_VAL_STRING:
			def = ti->var_string_def;
			break;
		case SKETCHUP_VAL_ARRAY:
			def = ti->var_array_def;
			break;
		case SKETCHUP_VAL_OBJECT:
			def = ti->var_object_def;
			break;
		case SKETCHUP_VAL_RESOURCE:
			def = ti->var_resource_def;
			break;
		case SKETCHUP_VAL_REFERENCE:
			def = ti->var_reference_def;
			break;
		default:
			def = ti->var_def;
			break;
	}

	SUComponentInstanceRef var = SU_INVALID;
	if (!sup_component_def_create_instance(ti->model, def, &var)) return false;
	SU_CALL_RETURN(SUComponentInstanceSetName(var, name));

	// TODO Create a SUTextRef to display name & var data

	// Calculate var location relative to room
	double wall_depth_y = room_index ? WALL_DEPTH /* Rooms have only one wall on y axis */ : WALL_DEPTH_TC * 2;
	double room_height = ti->room_bbox.max_point.y - wall_depth_y;
	double var_height = ti->var_bbox.max_point.y + VAR_PAD;
	size_t max_per_column = (size_t) (room_height / var_height);

	struct SUVector3D room_point = {0.0};
	sketchup_room_location(ti, room_index, visit_index, &room_point);

	struct SUVector3D point = {0.0};
	double var_width = ti->var_bbox.max_point.x + VAR_PAD;
	double wall_depth_x = room_index ? WALL_DEPTH : WALL_DEPTH_TC;
	point.x = room_point.x + wall_depth_x + ROOM_PAD + var_width * (double) (var_index / max_per_column);

	double room_top = room_point.y /* South */ + ti->room_bbox.max_point.y;
	point.y = room_top - wall_depth_x - ROOM_PAD - (var_height * (double) (var_index % max_per_column));

	point.z = room_point.z + (room_index ? FLOOR_PAD : FLOOR_PAD_TC);

	if (!sup_component_instance_move(var, point)) return false;
	return true;
}

#define HUMAN_HEIGHT_INCHES 72.0

bool sketchup_town_save(sketchup_town town, const char *file) {
	sup_town_impl *ti = TI(town);

	// Set the scene
	SUSceneRef scene = SU_INVALID;
	SU_CALL_RETURN(SUSceneCreate(&scene));
	int scene_index = 0;
	SU_CALL_RETURN(SUModelAddScene(ti->model, scene_index, scene, &scene_index));
	SU_CALL_RETURN(SUSceneSetName(scene, "PHP"));
	SU_CALL_RETURN(SUModelSetActiveScene(ti->model, scene));

	// Load styles
	SUStylesRef model_styles = SU_INVALID;
	SU_CALL_RETURN(SUModelGetStyles(ti->model, &model_styles));
	SU_CALL_RETURN(SUStylesAddStyle(model_styles, "models/style.style", true));

	// Camera
	SU_CALL_RETURN(SUSceneSetUseCamera(scene, true));
	SUCameraRef camera = SU_INVALID;
	SU_CALL_RETURN(SUSceneGetCamera(scene, &camera));
	const struct SUPoint3D position = {(-BUILDING_PAD + 42), (-BUILDING_PAD + 42), HUMAN_HEIGHT_INCHES};
	const struct SUPoint3D target = ti->town_center_bbox.max_point;
	const struct SUVector3D up_vector = {0.0, 0.0, 1.0};
	SU_CALL_RETURN(SUCameraSetOrientation(camera, &position, &target, &up_vector));

	// Uncomment to have camera set up when you open the file
	//SU_CALL_RETURN(SUSceneActivate(scene));

	enum SUResult res = SUModelSaveToFileWithVersion(ti->model, file, SUModelVersion_SU2021);
	return (res == SU_ERROR_NONE);
}

void sketchup_sdk_version(size_t bufsiz, char *version) {
	size_t major = 0;
	size_t minor = 0;
	SUGetAPIVersion(&major, &minor);
	snprintf(version, bufsiz, "%zu.%zu", major, minor);
}
