//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <cstdio>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr);                 \
    if (!n) {                                                  \
        n = (r (*) p)xdl_dsym(handle, #n, nullptr);            \
    }                                                          \
    if (!n) {                                                  \
        LOGW("api not found %s", #n);                          \
    }                                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

struct MapSeg {
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    char perms[8];
    std::string path;
};

static std::vector<MapSeg> parse_self_maps() {
    std::vector<MapSeg> segs;
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        LOGE("open /proc/self/maps failed");
        return segs;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        MapSeg s{};
        char path[768] = {};
        if (sscanf(line, "%" SCNx64 "-%" SCNx64 " %7s %" SCNx64 " %*s %*d %767[^\n]",
                   &s.start, &s.end, s.perms, &s.offset, path) < 4) {
            continue;
        }
        s.path = path;
        while (!s.path.empty() && s.path.front() == ' ') {
            s.path.erase(s.path.begin());
        }
        segs.push_back(s);
    }
    fclose(fp);
    return segs;
}

static bool write_mem_to_file(const char *outPath, uint64_t start, uint64_t size) {
    FILE *out = fopen(outPath, "wb");
    if (!out) {
        LOGE("open %s failed", outPath);
        return false;
    }
    const size_t wrote = fwrite(reinterpret_cast<const void *>(start), 1, (size_t) size, out);
    fclose(out);
    LOGI("wrote %s size=%zu", outPath, wrote);
    return wrote == (size_t) size;
}

static bool is_il2cpp_map(const MapSeg &s) {
    return s.path.find("libil2cpp.so") != std::string::npos;
}

static void restore_elf_header_from_disk(FILE *out, const std::string &soPath) {
    if (soPath.empty() || soPath[0] != '/') {
        return;
    }
    FILE *disk = fopen(soPath.c_str(), "rb");
    if (!disk) {
        LOGW("open disk so failed: %s", soPath.c_str());
        return;
    }
    unsigned char hdr[512];
    const size_t n = fread(hdr, 1, sizeof(hdr), disk);
    fclose(disk);
    if (n < 64 || hdr[0] != 0x7F || hdr[1] != 0x45 || hdr[2] != 0x4C || hdr[3] != 0x46) {
        LOGW("disk so has no ELF header");
        return;
    }
    if (fseeko(out, 0, SEEK_SET) != 0) {
        return;
    }
    fwrite(hdr, 1, n, out);
    LOGI("restored ELF header %zu bytes from %s", n, soPath.c_str());
}

static void collect_il2cpp_va_segs(const std::vector<MapSeg> &segs,
                                  std::vector<const MapSeg *> *out_segs,
                                  uint64_t *out_base,
                                  uint64_t *out_last,
                                  std::string *out_disk_path) {
    uint64_t named_min = 0;
    uint64_t named_max = 0;
    std::string disk_path;
    for (const auto &s : segs) {
        if (!is_il2cpp_map(s)) {
            continue;
        }
        LOGI("il2cpp map %s %" PRIx64 "-%" PRIx64 " off=%" PRIx64,
             s.perms, s.start, s.end, s.offset);
        if (!named_min || s.start < named_min) {
            named_min = s.start;
        }
        if (s.end > named_max) {
            named_max = s.end;
        }
        if (disk_path.empty() && s.offset == 0 && s.path[0] == '/') {
            disk_path = s.path;
        }
    }
    if (!named_min) {
        return;
    }
    for (const auto &s : segs) {
        if (s.perms[0] != 'r') {
            continue;
        }
        if (s.end <= named_min || s.start >= named_max) {
            continue;
        }
        if (!is_il2cpp_map(s) && !s.path.empty() && s.path[0] != '[') {
            continue;
        }
        out_segs->push_back(&s);
        if (s.end > named_max && is_il2cpp_map(s)) {
            named_max = s.end;
        }
    }
    *out_base = named_min;
    *out_last = named_max;
    *out_disk_path = disk_path;
}

static void scan_il2cpp_registrations(const std::vector<const MapSeg *> &segs, uint64_t base) {
    constexpr uint64_t kTypes = 53197;
    constexpr uint64_t kImages = 146;
    auto readable = [&](uint64_t va, uint64_t bytes) -> bool {
        for (auto *s : segs) {
            if (va >= s->start && va + bytes <= s->end) {
                return true;
            }
        }
        return false;
    };
    for (auto *s : segs) {
        if ((s->start & 7) != 0) {
            continue;
        }
        const auto *p = reinterpret_cast<const uint64_t *>(s->start);
        const size_t n = (size_t) ((s->end - s->start) / 8);
        for (size_t i = 0; i + 3 < n; ++i) {
            if (p[i] == kTypes && p[i + 2] == kTypes) {
                const uint64_t count_va = s->start + i * 8;
                const uint64_t mr_va = count_va - 80;
                LOGI("MetadataRegistration cand va=%" PRIx64 " rva=%" PRIx64,
                     mr_va, mr_va - base);
            }
            if (p[i] == kImages && readable(p[i + 1], 16)) {
                const uint64_t mods = p[i + 1];
                const auto *arr = reinterpret_cast<const uint64_t *>(mods);
                if (readable(arr[0], 16)) {
                    const auto *mod = reinterpret_cast<const uint64_t *>(arr[0]);
                    const auto name_va = mod[0];
                    if (readable(name_va, 8)) {
                        const auto *name = reinterpret_cast<const char *>(name_va);
                        if (strstr(name, ".dll") || strstr(name, "__Generated")) {
                            const uint64_t count_va = s->start + i * 8;
                            LOGI("CodeRegistration cand name=%s count_va=%" PRIx64
                                 " CR13_rva=%" PRIx64 " CR14_rva=%" PRIx64,
                                 name, count_va,
                                 count_va - base - 13 * 8,
                                 count_va - base - 14 * 8);
                        }
                    }
                }
            }
        }
    }
}

static void dump_so_from_maps(const char *outDir, const std::vector<MapSeg> &segs) {
    std::vector<const MapSeg *> va_segs;
    uint64_t base = 0;
    uint64_t last = 0;
    std::string disk_path;
    collect_il2cpp_va_segs(segs, &va_segs, &base, &last, &disk_path);
    if (va_segs.empty() || last <= base) {
        LOGE("no libil2cpp.so maps");
        return;
    }
    const uint64_t va_size = last - base;
    LOGI("il2cpp dump address (use this in Il2CppDumper): 0x%" PRIx64, base);
    LOGI("libil2cpp_va.so span %" PRIx64 "-%" PRIx64 " size=%" PRIu64, base, last, va_size);

    auto vaPath = std::string(outDir) + "/files/libil2cpp_va.so";
    FILE *out = fopen(vaPath.c_str(), "wb");
    if (!out) {
        LOGE("open %s failed", vaPath.c_str());
        return;
    }
    if (ftruncate(fileno(out), (off_t) va_size) != 0) {
        LOGW("ftruncate libil2cpp_va.so to %" PRIu64 " failed", va_size);
    }
    for (auto *s : va_segs) {
        const uint64_t off = s->start - base;
        const size_t n = (size_t) (s->end - s->start);
        if (fseeko(out, (off_t) off, SEEK_SET) != 0) {
            LOGE("fseeko va-off=%" PRIx64 " failed", off);
            continue;
        }
        const size_t wrote = fwrite(reinterpret_cast<const void *>(s->start), 1, n, out);
        LOGI("va dump mem=%" PRIx64 "-%" PRIx64 " file_off=%" PRIx64 " wrote=%zu %s",
             s->start, s->end, off, wrote, s->path.c_str());
    }
    restore_elf_header_from_disk(out, disk_path);
    fclose(out);
    LOGI("libil2cpp_va.so done, dump_address=0x%" PRIx64 " file_size=%" PRIu64, base, va_size);
    scan_il2cpp_registrations(va_segs, base);
}

static void dump_metadata_from_maps(const char *outDir, const std::vector<MapSeg> &segs) {
    constexpr uint32_t kMetaMagic = 0xFAB11BAF;
    int idx = 0;
    for (const auto &s : segs) {
        if (s.perms[0] != 'r') {
            continue;
        }
        const uint64_t size = s.end - s.start;
        const bool named = s.path.find("global-metadata") != std::string::npos
                           || s.path.find("Metadata") != std::string::npos;
        const bool anon_candidate = s.path.empty()
                                    || s.path[0] == '['
                                    || s.path.find("anon") != std::string::npos;
        if (!named && !(anon_candidate && size >= (1u << 20) && size <= (80u << 20))) {
            continue;
        }
        auto *p = reinterpret_cast<const uint32_t *>(s.start);
        if (*p != kMetaMagic) {
            continue;
        }
        LOGI("metadata magic at %" PRIx64 " size=%" PRIx64 " path=%s",
             s.start, size, s.path.c_str());
        char name[64];
        snprintf(name, sizeof(name), "/files/global-metadata_%d.dat", idx++);
        write_mem_to_file((std::string(outDir) + name).c_str(), s.start, size);
    }
    if (idx == 0) {
        LOGW("global-metadata.dat not found in maps (magic 0xFAB11BAF)");
    }
}

static void dump_runtime_binaries(const char *outDir) {
    LOGI("dump runtime binaries to %s/files/", outDir);
    auto segs = parse_self_maps();
    dump_so_from_maps(outDir, segs);
    dump_metadata_from_maps(outDir, segs);
}

static bool il2cpp_dump_apis_ready() {
    return il2cpp_domain_get
           && il2cpp_domain_get_assemblies
           && il2cpp_assembly_get_image
           && il2cpp_image_get_name
           && il2cpp_class_get_type
           && ((il2cpp_image_get_class && il2cpp_image_get_class_count)
               || (il2cpp_get_corlib && il2cpp_class_from_name
                   && il2cpp_class_get_method_from_name && il2cpp_class_from_system_type
                   && il2cpp_string_new));
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    if (!il2cpp_is_vm_thread) {
        LOGE("il2cpp_is_vm_thread missing, skip dump");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    // 易盾可能在 init 之后才把剩余导出填回去，再解析一次
    init_il2cpp_api(handle);
    if (!il2cpp_dump_apis_ready()) {
        LOGE("il2cpp dump APIs still missing after init (packed/stripped so), skip dump");
        return;
    }
    auto domain = il2cpp_domain_get();
    if (!domain || !il2cpp_thread_attach) {
        LOGE("il2cpp domain/thread_attach missing, skip dump");
        return;
    }
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    if (!il2cpp_dump_apis_ready()) {
        LOGE("skip dump: required il2cpp APIs are null");
        dump_runtime_binaries(outDir);
        return;
    }
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done!");
}
