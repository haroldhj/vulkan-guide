﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <functional>
#include <deque>
#include <memory>
#include <vk_mesh.h>
#include <vk_scene.h>
#include <vk_shaders.h>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL_events.h>
#include <frustum_cull.h>

namespace tracy { class VkCtx; }


namespace assets { struct PrefabInfo; }


//forward declarations
namespace vkutil {
	class DescriptorLayoutCache;
	class DescriptorAllocator;
}

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	void clear_vertex_input();

	void setShaders(struct ShaderEffect* effect);
};

class ComputePipelineBuilder {
public:

	VkPipelineShaderStageCreateInfo  _shaderStage;
	VkPipelineLayout _pipelineLayout;
	VkPipeline build_pipeline(VkDevice device);
};



struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

	template<typename F>
    void push_function(F&& function) {
		static_assert(sizeof(F) < 200, "DONT CAPTURE TOO MUCH IN THE LAMBDA");
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call functors
        }

        deletors.clear();
    }
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};


struct Material {
	VkDescriptorSet textureSet{VK_NULL_HANDLE};
	VkPipeline forwardPipeline{ VK_NULL_HANDLE };
	VkPipeline shadowPipeline{ VK_NULL_HANDLE };

	std::vector<std::string> textures;

	struct ShaderEffect* forwardEffect{nullptr};
	struct ShaderEffect* shadowEffect{ nullptr };
};

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
};



struct MeshObject {
	Mesh* mesh{ nullptr };

	Material* material{nullptr};

	uint32_t customSortKey;
	glm::mat4 transformMatrix;

	RenderBounds bounds;
};


struct FrameData {
	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _frameDeletionQueue;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	
	AllocatedBufferUntyped dynamicDataBuffer;

	vkutil::DescriptorAllocator* dynamicDescriptorAllocator;
};


struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;	
};

struct GPUCameraData{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
};


struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
	glm::vec4 origin_rad; // bounds
	glm::vec4 extents;  // bounds
};

struct PlayerCamera {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 inputAxis;

	float pitch{0}; //up-down rotation
	float yaw{0}; //left-right rotation

	glm::mat4 get_rotation_matrix();
};

struct EngineStats {
	float frametime;
	int objects;
	int drawcalls;
	int draws;
	int triangles;
};


struct MeshDrawCommands {
	struct RenderBatch {
		MeshObject* object;
		uint64_t sortKey;
		uint64_t objectIndex;
	};

	std::vector<RenderBatch> batch;
};

struct /*alignas(16)*/DrawCullData
{
	glm::mat4 viewMat;
	float P00, P11, znear, zfar; // symmetric projection parameters
	float frustum[4]; // data for left/right/top/bottom frustum planes
	float lodBase, lodStep; // lod distance i = base * pow(step, i)
	float pyramidWidth, pyramidHeight; // depth pyramid size in texels

	uint32_t drawCount;

	int cullingEnabled;
	int lodEnabled;
	int occlusionEnabled;
};

struct EngineConfig {
	float drawDistance{5000};
	bool occlusionCullGPU{ true };
	bool frustrumCullCPU { true };
};

constexpr unsigned int FRAME_OVERLAP = 2;
const int MAX_OBJECTS = 150000;
class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	int _selectedShader{ 0 };

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;

	VkPhysicalDeviceProperties _gpuProperties;

	FrameData _frames[FRAME_OVERLAP];
	
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	
	tracy::VkCtx* _graphicsQueueContext;

	VkRenderPass _renderPass;
	VkRenderPass _shadowPass;
	VkRenderPass _copyPass;

	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swachainImageFormat;

	VkFormat _renderFormat;
	AllocatedImage _rawRenderImage;
	VkSampler _smoothSampler;
	VkFramebuffer _forwardFramebuffer;
	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;	

    DeletionQueue _mainDeletionQueue;
	
	VmaAllocator _allocator; //vma lib allocator

	//depth resources
	
	AllocatedImage _depthImage;
	AllocatedImage _depthPyramid;

	int depthPyramidWidth ;
	int depthPyramidHeight;
	int depthPyramidLevels;
	
	//the format for the depth image
	VkFormat _depthFormat;
	
	vkutil::DescriptorAllocator* _descriptorAllocator;
	vkutil::DescriptorLayoutCache* _descriptorLayoutCache;

	VkDescriptorSetLayout _singleTextureSetLayout;

	GPUSceneData _sceneParameters;

	std::vector<VkBufferMemoryBarrier> uploadBarriers;

	UploadContext _uploadContext;

	PlayerCamera _camera;

	VkPipeline _cullPipeline;
	VkPipelineLayout _cullLayout;

	VkPipeline _depthReducePipeline;
	VkPipelineLayout _depthReduceLayout;

	VkPipeline _sparseUploadPipeline;
	VkPipelineLayout _sparseUploadLayout;

	VkPipeline _blitPipeline;
	VkPipelineLayout _blitLayout;

	VkSampler _depthSampler;
	VkImageView depthPyramidMips[16] = {};

	MeshDrawCommands currentCommands;
	RenderScene _renderScene;

	EngineConfig _config;

	void ready_mesh_draw(VkCommandBuffer cmd);
	
	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	void forward_pass(VkClearValue clearValue, VkCommandBuffer cmd);

	

	//run main loop
	void run();
	
	FrameData& get_current_frame();
	FrameData& get_last_frame();

	ShaderCache _shaderCache;
	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;
	std::unordered_map<std::string, Texture> _loadedTextures;
	std::unordered_map<std::string, assets::PrefabInfo*> _prefabCache;
	//functions

	//create material and add it to the map

	Material* create_material(VkPipeline pipeline, ShaderEffect* effect, const std::string& name);

	Material* clone_material(const std::string& originalname, const std::string& copyname);

	//returns nullptr if it cant be found
	Material* get_material(const std::string& name);

	//returns nullptr if it cant be found
	Mesh* get_mesh(const std::string& name);

	//our draw function
	void draw_objects(VkCommandBuffer cmd, RenderScene::MeshPass& pass);

	void reduce_depth(VkCommandBuffer cmd);

	glm::mat4 get_view_matrix();


	glm::mat4 get_projection_matrix(bool bReverse = true);

	void execute_compute_cull(VkCommandBuffer cmd, RenderScene::MeshPass& pass);

	AllocatedBufferUntyped create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags = 0);

	size_t pad_uniform_buffer_size(size_t originalSize);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	bool load_prefab(const char* path, glm::mat4 root);

	std::string asset_path(const char* path);
	std::string asset_path(std::string& path);

	std::string shader_path(const char* path);
	std::string shader_path(std::string& path);

	void refresh_renderbounds(MeshObject* object);

	template<typename T>
	T* map_buffer(AllocatedBuffer<T> &buffer);
	
	void unmap_buffer(AllocatedBufferUntyped& buffer);

	bool load_compute_shader(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout);
private:
	EngineStats stats;
	void process_input_event(SDL_Event* ev);
	void update_camera(float deltaSeconds);

	void init_vulkan();

	void init_swapchain();

	void init_forward_renderpass();

	void init_copy_renderpass();

	void init_shadow_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

	void init_pipelines();

	void fill_forward_pipeline(PipelineBuilder& pipelineBuilder);
	void fill_shadow_pipeline(PipelineBuilder& pipelineBuilder);

	void init_scene();

	void build_texture_set(VkSampler blockySampler, Material* texturedMat, const char* textureName);

	void init_descriptors();

	void init_imgui();	

	void load_meshes();

	void load_images();

	bool load_image_to_cache(const char* name, const char* path);

	void upload_mesh(Mesh& mesh);

	void copy_render_to_swapchain(uint32_t swapchainImageIndex, VkCommandBuffer cmd);
};

template<typename T>
T* VulkanEngine::map_buffer(AllocatedBuffer<T>& buffer)
{
	void* data;
	vmaMapMemory(_allocator, buffer._allocation, &data);
	return(T*)data;
}
