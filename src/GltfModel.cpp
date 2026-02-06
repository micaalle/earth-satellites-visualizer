#include "GltfModel.h"
#include "Shader.h"

#include <iostream>
#include <algorithm>
#include <cmath>

#include "cgltf.h"

#include "stb_image.h"

struct Vtx {
    glm::vec3 p;
    glm::vec3 n;
    glm::vec2 uv;
};

static cgltf_accessor* findAttr(cgltf_primitive* prim, cgltf_attribute_type type, int index = 0) {
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        cgltf_attribute& a = prim->attributes[i];
        if (a.type == type && (int)a.index == index) return a.data;
    }
    return nullptr;
}

GLuint GltfModel::createTextureFromMemoryRGBA(const unsigned char* rgba, int w, int h, bool srgb) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool GltfModel::loadFromFile(const std::string& path, bool srgbBaseColor) {
    destroy();

    cgltf_options options{};
    cgltf_data* data = nullptr;

    cgltf_result r = cgltf_parse_file(&options, path.c_str(), &data);
    if (r != cgltf_result_success || !data) {
        std::cerr << "cgltf_parse_file failed: " << path << "\n";
        return false;
    }

    r = cgltf_load_buffers(&options, data, path.c_str());
    if (r != cgltf_result_success) {
        std::cerr << "cgltf_load_buffers failed: " << path << "\n";
        cgltf_free(data);
        return false;
    }

    m_textures.resize(data->images_count);
    for (cgltf_size i = 0; i < data->images_count; i++) {
        cgltf_image* img = &data->images[i];

        const unsigned char* bytes = nullptr;
        int byteCount = 0;

        std::string externalPath;

        if (img->buffer_view) {
            cgltf_buffer_view* bv = img->buffer_view;
            bytes = (const unsigned char*)bv->buffer->data + bv->offset;
            byteCount = (int)bv->size;
        } else if (img->uri) {
            std::string baseDir = path;
            size_t slash = baseDir.find_last_of("/\\");
            baseDir = (slash == std::string::npos) ? "" : baseDir.substr(0, slash + 1);
            externalPath = baseDir + img->uri;
        } else {
            continue;
        }

        int w=0,h=0,comp=0;
        unsigned char* rgba = nullptr;

        if (bytes && byteCount > 0) {
            rgba = stbi_load_from_memory(bytes, byteCount, &w, &h, &comp, 4);
        } else if (!externalPath.empty()) {
            rgba = stbi_load(externalPath.c_str(), &w, &h, &comp, 4);
        }

        if (!rgba || w <= 0 || h <= 0) {
            if (rgba) stbi_image_free(rgba);
            continue;
        }

        GLuint tex = createTextureFromMemoryRGBA(rgba, w, h, srgbBaseColor);
        stbi_image_free(rgba);

        m_textures[i].id = tex;
        m_textures[i].valid = true;
    }

    float maxR = 0.0f;

    for (cgltf_size mi = 0; mi < data->meshes_count; mi++) {
        cgltf_mesh* mesh = &data->meshes[mi];

        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
            cgltf_primitive* prim = &mesh->primitives[pi];

            cgltf_accessor* posAcc = findAttr(prim, cgltf_attribute_type_position, 0);
            if (!posAcc) continue;

            cgltf_accessor* nrmAcc = findAttr(prim, cgltf_attribute_type_normal, 0);
            cgltf_accessor* uvAcc  = findAttr(prim, cgltf_attribute_type_texcoord, 0);

            const cgltf_size vcount = posAcc->count;
            std::vector<Vtx> verts;
            verts.resize((size_t)vcount);

            for (cgltf_size v = 0; v < vcount; v++) {
                float p[3]{0,0,0};
                float n[3]{0,1,0};
                float uv[2]{0,0};

                cgltf_accessor_read_float(posAcc, v, p, 3);
                if (nrmAcc) cgltf_accessor_read_float(nrmAcc, v, n, 3);
                if (uvAcc)  cgltf_accessor_read_float(uvAcc,  v, uv, 2);

                verts[(size_t)v].p  = glm::vec3(p[0], p[1], p[2]);
                verts[(size_t)v].n  = glm::normalize(glm::vec3(n[0], n[1], n[2]));
                verts[(size_t)v].uv = glm::vec2(uv[0], uv[1]);

                maxR = std::max(maxR, glm::length(verts[(size_t)v].p));
            }

            std::vector<uint32_t> indices;
            bool hasIdx = (prim->indices != nullptr);

            if (hasIdx) {
                cgltf_accessor* idxAcc = prim->indices;
                indices.resize((size_t)idxAcc->count);
                for (cgltf_size k = 0; k < idxAcc->count; k++) {
                    indices[(size_t)k] = (uint32_t)cgltf_accessor_read_index(idxAcc, k);
                }
            }

            Primitive out{};
            out.mode = (prim->type == cgltf_primitive_type_triangles) ? GL_TRIANGLES : GL_TRIANGLES;
            out.hasIndices = hasIdx;
            out.indexCount = hasIdx ? (GLsizei)indices.size() : (GLsizei)verts.size();

            out.baseColorTexIndex = -1;
            if (prim->material) {
                auto& pbr = prim->material->pbr_metallic_roughness;
                if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {

                    cgltf_image* img = pbr.base_color_texture.texture->image;
                    ptrdiff_t idx = img - data->images;
                    if (idx >= 0 && idx < (ptrdiff_t)m_textures.size() && m_textures[(size_t)idx].valid) {
                        out.baseColorTexIndex = (int)idx;
                    }
                }
            }

            glGenVertexArrays(1, &out.vao);
            glGenBuffers(1, &out.vbo);
            glGenBuffers(1, &out.ebo);

            glBindVertexArray(out.vao);

            glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vtx)), verts.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, p));

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, n));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, uv));

            if (hasIdx) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
            }

            glBindVertexArray(0);

            m_prims.push_back(out);
        }
    }

    m_boundsRadius = (maxR > 1e-6f) ? maxR : 1.0f;

    cgltf_free(data);
    return !m_prims.empty();
}

void GltfModel::drawEarthStyle(Shader& earthShader, int textureUnit) const {
    for (const auto& p : m_prims) {
        bool hasTex = (p.baseColorTexIndex >= 0 &&
                       (size_t)p.baseColorTexIndex < m_textures.size() &&
                       m_textures[(size_t)p.baseColorTexIndex].valid);

        earthShader.setBool("uHasTex", hasTex);
        earthShader.setInt("uEarthTex", textureUnit);

        if (hasTex) {
            glActiveTexture(GL_TEXTURE0 + textureUnit);
            glBindTexture(GL_TEXTURE_2D, m_textures[(size_t)p.baseColorTexIndex].id);
        }

        glBindVertexArray(p.vao);
        if (p.hasIndices) {
            glDrawElements(p.mode, p.indexCount, GL_UNSIGNED_INT, (void*)0);
        } else {
            glDrawArrays(p.mode, 0, p.indexCount);
        }
        glBindVertexArray(0);

        if (hasTex) glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void GltfModel::destroy() {
    for (auto& p : m_prims) {
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
        if (p.vbo) glDeleteBuffers(1, &p.vbo);
        if (p.vao) glDeleteVertexArrays(1, &p.vao);
        p = Primitive{};
    }
    m_prims.clear();

    for (auto& t : m_textures) {
        if (t.id) glDeleteTextures(1, &t.id);
        t = Texture{};
    }
    m_textures.clear();

    m_boundsRadius = 1.0f;
}
