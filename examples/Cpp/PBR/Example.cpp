/*
 * Example.cpp (Example_PBR)
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include <ExampleBase.h>
#include <FileUtils.h>
#include <stb/stb_image.h>


class Example_PBR : public ExampleBase
{

    LLGL::Buffer*               vertexBuffer        = nullptr;
    LLGL::Buffer*               constantBuffer      = nullptr;

    ShaderPipeline              shaderPipelineMeshes;
    LLGL::PipelineLayout*       layoutMeshes        = nullptr;
    LLGL::PipelineState*        pipelineMeshes      = nullptr;

    ShaderPipeline              shaderPipelineSky;
    LLGL::PipelineLayout*       layoutSky           = nullptr;
    LLGL::PipelineState*        pipelineSky         = nullptr;

    LLGL::Texture*              skyboxArray         = nullptr;
    LLGL::Texture*              colorMapArray       = nullptr;
    LLGL::Texture*              normalMapArray      = nullptr;
    LLGL::Texture*              roughnessMapArray   = nullptr;
    LLGL::Texture*              metallicMapArray    = nullptr;

    LLGL::Sampler*              linearSampler       = nullptr;

    LLGL::ResourceHeap*         resourceHeapMeshes  = nullptr;
    LLGL::ResourceHeap*         resourceHeapSkybox  = nullptr;

    std::vector<TriangleMesh>   meshes;

    struct Settings
    {
        Gs::Matrix4f    cMatrix;
        Gs::Matrix4f    vpMatrix;
        Gs::Matrix4f    wMatrix;
        Gs::Vector2f    aspectRatio;
        float           mipCount;
        float           _pad0;
        Gs::Vector4f    lightDir        = { 0, 0, -1, 0 };
        std::uint32_t   skyboxLayer     = 0;
        std::uint32_t   materialLayer   = 0;//1;
        std::uint32_t   _pad1[2];
    }
    settings;

    const int                   defaultImageSize    = 1024;
    int                         currentMesh         = 0;
    float                       viewPitch           = 0.0f;
    float                       viewYaw             = 0.0f;

    std::uint32_t               numSkyboxes         = 0;
    std::uint32_t               numMaterials        = 0;

public:

    Example_PBR() :
        ExampleBase { "LLGL Example: PBR" }
    {
        // Validate required rendering capabilities
        LLGL::RenderingCapabilities caps;
        {
            caps.features.hasArrayTextures      = true;
            caps.features.hasCubeArrayTextures  = true;
        }
        LLGL::ValidateRenderingCaps(renderer->GetRenderingCaps(), caps);

        // Create all graphics objects
        auto vertexFormat = CreateBuffers();
        LoadShaders(vertexFormat);
        CreatePipelines();
        CreateTextures();
        CreateResourceHeaps();

        // Print some information on the standard output
        std::cout << "press TAB KEY to switch between five different texture samplers" << std::endl;
    }

private:

    LLGL::VertexFormat CreateBuffers()
    {
        // Specify vertex format
        LLGL::VertexFormat vertexFormat;
        vertexFormat.AppendAttribute({ "position",  LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "normal",    LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "tangent",   LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "bitangent", LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "texCoord",  LLGL::Format::RG32Float  });

        // Load 3D models
        std::vector<TexturedVertex> vertices;
        meshes.push_back(LoadObjModel(vertices, "UVSphere.obj"));
        meshes.push_back(LoadObjModel(vertices, "WiredBox.obj"));

        // Create vertex and constant buffer
        vertexBuffer = CreateVertexBuffer(GenerateTangentSpaceVertices(vertices), vertexFormat);
        constantBuffer = CreateConstantBuffer(settings);

        return vertexFormat;
    }

    void LoadShaders(const LLGL::VertexFormat& vertexFormat)
    {
        if (Supported(LLGL::ShadingLanguage::HLSL))
        {
            shaderPipelineSky.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.hlsl", "VSky", "vs_5_0" });
            shaderPipelineSky.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.hlsl", "PSky", "ps_5_0" });

            shaderPipelineMeshes.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.hlsl", "VMesh", "vs_5_0" }, { vertexFormat });
            shaderPipelineMeshes.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.hlsl", "PMesh", "ps_5_0" });
        }
        else if (Supported(LLGL::ShadingLanguage::GLSL))
        {
            shaderPipelineSky.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.Sky.vert" });
            shaderPipelineSky.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.Sky.frag" });

            shaderPipelineMeshes.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.Mesh.vert" }, { vertexFormat });
            shaderPipelineMeshes.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.Mesh.frag" });
        }
        else if (Supported(LLGL::ShadingLanguage::Metal))
        {
            shaderPipelineSky.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.metal", "VSky", "1.1" });
            shaderPipelineSky.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.metal", "PSky", "1.1" });

            shaderPipelineMeshes.vs = LoadShader({ LLGL::ShaderType::Vertex,   "Example.metal", "VMesh", "1.1" }, { vertexFormat });
            shaderPipelineMeshes.ps = LoadShader({ LLGL::ShaderType::Fragment, "Example.metal", "PMesh", "1.1" });
        }
        else
            throw std::runtime_error("shaders not supported for active renderer");
    }

    void CreatePipelines()
    {
        // Create pipeline layout for skybox
        if (IsOpenGL())
        {
            layoutSky = renderer->CreatePipelineLayout(
                LLGL::Parse(
                    "heap{"
                    "cbuffer(Settings@1):frag:vert,"
                    "sampler(skyBox@2):frag,"
                    "texture(2):frag,"
                    "}"
                )
            );
        }
        else
        {
            layoutSky = renderer->CreatePipelineLayout(
                LLGL::Parse(
                    "heap{"
                    "cbuffer(1):frag:vert,"
                    "sampler(2):frag,"
                    "texture(3):frag,"
                    "}"
                )
            );
        }

        // Create graphics pipeline for skybox
        LLGL::GraphicsPipelineDescriptor pipelineDescSky;
        {
            pipelineDescSky.vertexShader                    = shaderPipelineSky.vs;
            pipelineDescSky.fragmentShader                  = shaderPipelineSky.ps;
            pipelineDescSky.pipelineLayout                  = layoutSky;
            //pipelineDescSky.depth.testEnabled               = true;
            //pipelineDescSky.depth.writeEnabled              = true;
            pipelineDescSky.rasterizer.multiSampleEnabled   = (GetSampleCount() > 1);
        }
        pipelineSky = renderer->CreatePipelineState(pipelineDescSky);

        // Create pipeline layout for meshes
        if (IsOpenGL())
        {
            layoutMeshes = renderer->CreatePipelineLayout(
                LLGL::Parse(
                    "heap{"
                    "  cbuffer(Settings@1):frag:vert,"
                    "  sampler(skyBox@2, colorMaps@3, normalMaps@4, roughnessMaps@5, metallicMaps@6):frag,"
                    "  texture(2,3,4,5,6):frag,"
                    "}"
                )
            );
        }
        else
        {
            layoutMeshes = renderer->CreatePipelineLayout(
                LLGL::Parse(
                    "heap{"
                    "  cbuffer(1):frag:vert,"
                    "  sampler(2):frag,"
                    "  texture(3,4,5,6,7):frag,"
                    "}"
                )
            );
        }

        // Create graphics pipeline for meshes
        LLGL::GraphicsPipelineDescriptor pipelineDescMeshes;
        {
            pipelineDescMeshes.vertexShader                     = shaderPipelineMeshes.vs;
            pipelineDescMeshes.fragmentShader                   = shaderPipelineMeshes.ps;
            pipelineDescMeshes.pipelineLayout                   = layoutMeshes;
            pipelineDescMeshes.depth.testEnabled                = true;
            pipelineDescMeshes.depth.writeEnabled               = true;
            pipelineDescMeshes.rasterizer.multiSampleEnabled    = (GetSampleCount() > 1);
        }
        pipelineMeshes = renderer->CreatePipelineState(pipelineDescMeshes);
    }

    void LoadImage(const std::string& filename, int& texWidth, int& texHeight, std::vector<std::uint8_t>& imageData)
    {
        // Print information about current texture
        const std::string path = FindResourcePath(filename);
        std::cout << "load image: \"" << path << "\"" << std::endl;

        // Load image data from file (using STBI library, see http://nothings.org/stb_image.h)
        int w = 0, h = 0, n = 0;
        unsigned char* buf = stbi_load(path.c_str(), &w, &h, &n, 4);
        if (!buf)
            throw std::runtime_error("failed to load image: \"" + filename + "\"");

        // Check if image size is compatible
        if (texWidth == 0)
        {
            texWidth    = w;
            texHeight   = h;
        }
        else if (w != texWidth || h != texHeight)
            throw std::runtime_error("size mismatch for texture array while loading image: \"" + filename + "\"");

        // Copy into array
        auto offset = imageData.size();
        auto bufSize = static_cast<std::size_t>(w*h*4);

        imageData.resize(offset + bufSize);
        ::memcpy(&(imageData[offset]), buf, bufSize);

        // Release image data
        stbi_image_free(buf);
    }

    void FillImage(int& texWidth, int& texHeight, std::vector<std::uint8_t>& imageData)
    {
        // Initialize texture size with default value if necessary
        if (texWidth == 0)
        {
            texWidth    = defaultImageSize;
            texHeight   = defaultImageSize;
        }

        // Fill image data
        auto offset = imageData.size();
        auto bufSize = static_cast<std::size_t>(texWidth*texHeight*4);

        imageData.resize(offset + bufSize, 0);
    }

    // Loads multiple images into one texture array or cube-map array
    LLGL::Texture* LoadTextureArray(const LLGL::TextureType texType, const std::initializer_list<std::string>& texFilenames)
    {
        // Load image data
        int texWidth = 0, texHeight = 0;
        std::vector<std::uint8_t> imageData;

        for (auto filename : texFilenames)
        {
            if (filename.empty())
                FillImage(texWidth, texHeight, imageData);
            else
                LoadImage("PBR/" + filename, texWidth, texHeight, imageData);
        }

        // Define initial texture data
        LLGL::ImageView srcImageView;
        {
            srcImageView.format     = LLGL::ImageFormat::RGBA;
            srcImageView.dataType   = LLGL::DataType::UInt8;
            srcImageView.data       = imageData.data();
            srcImageView.dataSize   = imageData.size();
        }

        // Create texture
        LLGL::TextureDescriptor texDesc;
        {
            texDesc.type            = texType;
            texDesc.format          = LLGL::Format::RGBA8UNorm;
            texDesc.extent.width    = static_cast<std::uint32_t>(texWidth);
            texDesc.extent.height   = static_cast<std::uint32_t>(texHeight);
            texDesc.extent.depth    = 1;
            texDesc.arrayLayers     = static_cast<std::uint32_t>(texFilenames.size());
        }
        auto tex = renderer->CreateTexture(texDesc, &srcImageView);

        return tex;
    }

    void CreateTextures()
    {
        numSkyboxes = 1;
        numMaterials = 4;

        // Load skybox texutres
        skyboxArray = LoadTextureArray(
            LLGL::TextureType::TextureCubeArray,
            {
                // 1st skybox "mp_alpha"
                "mp_alpha/alpha-island_rt.tga", // X+ = right
                "mp_alpha/alpha-island_lf.tga", // X- = left
                "mp_alpha/alpha-island_up.tga", // Y+ = up
                "mp_alpha/alpha-island_dn.tga", // Y- = down
                "mp_alpha/alpha-island_bk.tga", // Z+ = back
                "mp_alpha/alpha-island_ft.tga", // Z- = front
            }
        );

        // Store number of MIP-maps for environment map
        settings.mipCount = static_cast<float>(skyboxArray->GetDesc().mipLevels);

        // Load PBR textures
        colorMapArray = LoadTextureArray(
            LLGL::TextureType::Texture2DArray,
            {
                "Wood13/Wood13_col.jpg",
                "Tiles26/Tiles26_col.jpg",
                "Tiles22/Tiles22_col.jpg",
                "Metal04/Metal04_col.jpg",
            }
        );

        normalMapArray = LoadTextureArray(
            LLGL::TextureType::Texture2DArray,
            {
                "Wood13/Wood13_nrm.jpg",
                "Tiles26/Tiles26_nrm.jpg",
                "Tiles22/Tiles22_nrm.jpg",
                "Metal04/Metal04_nrm.jpg",
            }
        );

        roughnessMapArray = LoadTextureArray(
            LLGL::TextureType::Texture2DArray,
            {
                "Wood13/Wood13_rgh.jpg",
                "Tiles26/Tiles26_rgh.jpg",
                "Tiles22/Tiles22_rgh.jpg",
                "Metal04/Metal04_rgh.jpg",
            }
        );

        metallicMapArray = LoadTextureArray(
            LLGL::TextureType::Texture2DArray,
            {
                "",                         // non-metallic
                "",                         // non-metallic
                "",                         // non-metallic
                "Metal04/Metal04_met.jpg",  // metallic
            }
        );

        // Create linear sampler
        LLGL::SamplerDescriptor samplerDesc;
        {
            samplerDesc.maxAnisotropy = 8;
        }
        linearSampler = renderer->CreateSampler(samplerDesc);
    }

    void CreateResourceHeaps()
    {
        // Create resource heap for skybox
        const LLGL::ResourceViewDescriptor resourceViewsSky[] =
        {
            constantBuffer, linearSampler, skyboxArray
        };
        resourceHeapSkybox = renderer->CreateResourceHeap(layoutSky, resourceViewsSky);
        resourceHeapSkybox->SetDebugName("resourceHeapSkybox");

        // Create resource heap for meshes
        std::vector<LLGL::ResourceViewDescriptor> resourceViewsMeshes;
        if (IsOpenGL())
        {
            resourceViewsMeshes =
            {
                constantBuffer,
                linearSampler,
                linearSampler,
                linearSampler,
                linearSampler,
                linearSampler,
                skyboxArray,
                colorMapArray,
                normalMapArray,
                roughnessMapArray,
                metallicMapArray,
            };
        }
        else
        {
            resourceViewsMeshes =
            {
                constantBuffer,
                linearSampler,
                skyboxArray,
                colorMapArray,
                normalMapArray,
                roughnessMapArray,
                metallicMapArray,
            };
        }
        resourceHeapMeshes = renderer->CreateResourceHeap(layoutMeshes, resourceViewsMeshes);
        resourceHeapMeshes->SetDebugName("resourceHeapMeshes");
    }

private:

    void UpdateScene()
    {
        // Update camera rotation
        const auto motion = input.GetMouseMotion();
        const Gs::Vector2f motionVec
        {
            static_cast<float>(motion.x),
            static_cast<float>(motion.y)
        };

        if (input.KeyPressed(LLGL::Key::LButton))
        {
            if (input.KeyPressed(LLGL::Key::Space))
            {
                // Rotate mesh
                auto& m = meshes[currentMesh].transform;
                Gs::Matrix4f deltaRotation;
                Gs::RotateFree(deltaRotation, { 1, 0, 0 }, motionVec.y * 0.01f);
                Gs::RotateFree(deltaRotation, { 0, 1, 0 }, motionVec.x * 0.01f);
                m = deltaRotation * m;
            }
            else
            {
                // Rotate camera
                viewPitch   += motionVec.y * 0.25f;
                viewYaw     += motionVec.x * 0.25f;
                viewPitch = Gs::Clamp(viewPitch, -90.0f, 90.0f);
            }
        }

        // Update material and skybox layer switches
        if (input.KeyDown(LLGL::Key::Tab))
        {
            if (input.KeyPressed(LLGL::Key::Shift))
            {
                if (numSkyboxes > 0)
                    settings.skyboxLayer = (settings.skyboxLayer + 1) % numSkyboxes;
            }
            else if (input.KeyPressed(LLGL::Key::Space))
            {
                if (!meshes.empty())
                    currentMesh = (currentMesh + 1) % static_cast<int>(meshes.size());
            }
            else
            {
                if (numMaterials > 0)
                    settings.materialLayer = (settings.materialLayer + 1) % numMaterials;
            }
        }

        // Set projection, view, and world matrix
        Gs::Matrix4f viewMatrix;
        Gs::RotateFree(viewMatrix, Gs::Vector3f{ 0, 1, 0 }, Gs::Deg2Rad(viewYaw));
        Gs::RotateFree(viewMatrix, Gs::Vector3f{ 1, 0, 0 }, Gs::Deg2Rad(viewPitch));
        Gs::Translate(viewMatrix, Gs::Vector3f{ 0, 0, -4 });

        settings.vpMatrix = projection * viewMatrix.Inverse();
        settings.wMatrix = meshes[currentMesh].transform;

        settings.aspectRatio = { GetAspectRatio(), 1.0f };

        settings.cMatrix = viewMatrix;
    }

    void RenderSkybox()
    {
        commands->SetPipelineState(*pipelineSky);
        commands->SetResourceHeap(*resourceHeapSkybox);
        commands->Draw(3, 0);
    }

    void RenderMesh(const TriangleMesh& mesh)
    {
        commands->SetPipelineState(*pipelineMeshes);
        commands->SetResourceHeap(*resourceHeapMeshes);
        commands->Draw(mesh.numVertices, mesh.firstVertex);
    }

    void RenderScene()
    {
        commands->UpdateBuffer(*constantBuffer, 0, &settings, sizeof(settings));
        commands->BeginRenderPass(*swapChain);
        {
            commands->Clear(LLGL::ClearFlags::ColorDepth);
            commands->SetViewport(swapChain->GetResolution());
            commands->SetVertexBuffer(*vertexBuffer);

            RenderSkybox();
            RenderMesh(meshes[currentMesh]);
        }
        commands->EndRenderPass();
    }

    void OnDrawFrame() override
    {
        UpdateScene();

        commands->Begin();
        {
            RenderScene();
        }
        commands->End();

        commandQueue->Submit(*commands);
    }

};

LLGL_IMPLEMENT_EXAMPLE(Example_PBR);



