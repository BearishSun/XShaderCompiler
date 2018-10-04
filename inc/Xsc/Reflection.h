/*
 * Reflection.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_REFLECTION_H
#define XSC_REFLECTION_H


#include "Export.h"
#include <limits>
#include <string>
#include <map>
#include <vector>
#include <ostream>


namespace Xsc
{

//! Shader code reflection namespace
namespace Reflection
{


/* ===== Public enumerations ===== */

//! Sampler filter enumeration.
enum class Filter
{
    None                            = 1,
    Point                           = 2,
    Linear                          = 3,
    Anisotropic                     = 4,
};

//! Texture address mode enumeration.
enum class TextureAddressMode
{
    Wrap        = 1,
    Mirror      = 2,
    Clamp       = 3,
    Border      = 4,
    MirrorOnce  = 5,
};

//! Sample comparison function enumeration.
enum class ComparisonFunc
{
    Never           = 1,
    Less            = 2,
    Equal           = 3,
    LessEqual       = 4,
    Greater         = 5,
    NotEqual        = 6,
    GreaterEqual    = 7,
    Always          = 8,
};

//! Rasterizer fill mode.
enum class FillMode
{
    Wire            = 1,
    Solid           = 2,
};

//! Rasterizer cull mode.
enum class CullMode
{
    Clockwise           = 1,
    CounterClockwise    = 2,
    None                = 3,
};

//! Action to take on stencil operations.
enum class StencilOpType
{
    Keep                = 1,
    Zero                = 2,
    Replace             = 3,
    Increment           = 4,
    Decrement           = 5,
    IncrementWrap       = 6,
    DecrementWrap       = 7,
    Inverse             = 8,
};

//! Factor to apply to one of the operands during the blend operation.
enum class BlendFactor
{
    One                 = 1,
    Zero                = 2,
    DestinationRGB      = 3,
    SourceRGB           = 4,
    DestinationInvRGB   = 5,
    SourceInvRGB        = 6,
    DestinationA        = 7,
    SourceA             = 8,
    DestinationInvA     = 9,
    SourceInvA          = 10,
};

//! Operation to apply to the two operands during blending.
enum class BlendOpType
{
    Add                 = 1,
    Subtract            = 2,
    ReverseSubtract     = 3,
    Minimum             = 4,
    Maximum             = 5,
};

//! Option used for controlling in what order will elements with the shader be rendered in
enum class SortMode
{
    None                = 1,
    BackToFront         = 2,
    FrontToBack         = 3    
};


/* ===== Public structures ===== */

/**
\brief Sampler state descriptor structure
*/
struct SamplerState
{
    Filter              filterMin       = Filter::Linear;
    Filter              filterMax       = Filter::Linear;
    Filter              filterMip       = Filter::Linear;
    TextureAddressMode  addressU        = TextureAddressMode::Wrap;
    TextureAddressMode  addressV        = TextureAddressMode::Wrap;
    TextureAddressMode  addressW        = TextureAddressMode::Wrap;
    float               mipLODBias      = 0.0f;
    unsigned int        maxAnisotropy   = 1u;
    ComparisonFunc      comparisonFunc  = ComparisonFunc::Always;
    float               borderColor[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float               minLOD          = -std::numeric_limits<float>::max();
    float               maxLOD          = std::numeric_limits<float>::max();

    // BEGIN BANSHEE CHANGES
    bool                isNonDefault    = false;
    std::string         alias;
    // END BANSHEE CHANGES
};

//! Options controlling the rasterizer.
struct RasterizerState
{
    FillMode fillMode           = FillMode::Solid;
    CullMode cullMode           = CullMode::CounterClockwise;
    bool scissorEnable          = false;
    bool multisampleEnable      = true;
    bool antialisedLineEnable   = false;
};

//! Options controlling depth buffer operations.
struct DepthState
{
    bool readEnable             = true;
    bool writeEnable            = true;
    ComparisonFunc compareFunc  = ComparisonFunc::Less;
    float depthBias             = 0.0f;
    float scaledDepthBias       = 0.0f;
    bool depthClip              = true;
};

//! Per-face information about a stencil state.
struct StencilOperation
{
    StencilOpType fail          = StencilOpType::Keep;
    StencilOpType zfail         = StencilOpType::Keep;
    StencilOpType pass          = StencilOpType::Keep;
    ComparisonFunc compareFunc  = ComparisonFunc::Always;
};

//! Options controlling stencil buffer operations.
struct StencilState
{
    bool enabled                = false;
    int32_t reference           = 0;
    uint8_t readMask            = 0xFF;
    uint8_t writeMask           = 0xFF;
    StencilOperation front;
    StencilOperation back;
};

//! Options describing a blend operation on a subset of the render target.
struct BlendOperation
{
    BlendFactor source          = BlendFactor::One;
    BlendFactor destination     = BlendFactor::Zero;
    BlendOpType operation       = BlendOpType::Add;
};

//! Options controlling blend state for a single render target.
struct BlendStateTarget
{
    bool enabled                = false;
    int8_t writeMask            = 0b1111;
    BlendOperation colorOp;
    BlendOperation alphaOp;
};

//! Options controlling the blend state.
struct BlendState
{
    static constexpr uint32_t MAX_NUM_RENDER_TARGETS = 8;

    bool alphaToCoverage    = false;
    bool independantBlend   = false;

    BlendStateTarget targets[MAX_NUM_RENDER_TARGETS];
};

//! Global options for a shader
struct GlobalOptions
{
    SortMode sortMode       = SortMode::FrontToBack;
    bool separable          = false;
    bool transparent        = false;
    bool forward            = false;
    int32_t priority        = 0;
};

//! Binding slot of textures, constant buffers, and fragment targets.
struct BindingSlot
{
    //! Identifier of the binding point.
    std::string ident;

    //! Zero based binding point or location. If this is -1, the location has not been set.
    int         location;
};

// BEGIN BANSHEE CHANGES

enum class VariableType
{
    Buffer,
    UniformBuffer,
    Sampler,
    Variable,
    Struct
};

enum class BufferType
{
    Undefined,

    Buffer,
    StructuredBuffer,
    ByteAddressBuffer,

    RWBuffer,
    RWStructuredBuffer,
    RWByteAddressBuffer,
    AppendStructuredBuffer,
    ConsumeStructuredBuffer,

    RWTexture1D,
    RWTexture1DArray,
    RWTexture2D,
    RWTexture2DArray,
    RWTexture3D,

    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    Texture2DMS,
    Texture2DMSArray,
};

enum class DataType
{
    Undefined,

    // String types,
    String,

    // Scalar types
    Bool,
    Int,
    UInt,
    Half,
    Float,
    Double,
    
    // Vector types
    Bool2,
    Bool3,
    Bool4,
    Int2,
    Int3,
    Int4,
    UInt2,
    UInt3,
    UInt4,
    Half2,
    Half3,
    Half4,
    Float2,
    Float3,
    Float4,
    Double2,
    Double3,
    Double4,

    // Matrix types
    Bool2x2,
    Bool2x3,
    Bool2x4,
    Bool3x2,
    Bool3x3,
    Bool3x4,
    Bool4x2,
    Bool4x3,
    Bool4x4,
    Int2x2,
    Int2x3,
    Int2x4,
    Int3x2,
    Int3x3,
    Int3x4,
    Int4x2,
    Int4x3,
    Int4x4,
    UInt2x2,
    UInt2x3,
    UInt2x4,
    UInt3x2,
    UInt3x3,
    UInt3x4,
    UInt4x2,
    UInt4x3,
    UInt4x4,
    Half2x2,
    Half2x3,
    Half2x4,
    Half3x2,
    Half3x3,
    Half3x4,
    Half4x2,
    Half4x3,
    Half4x4,
    Float2x2,
    Float2x3,
    Float2x4,
    Float3x2,
    Float3x3,
    Float3x4,
    Float4x2,
    Float4x3,
    Float4x4,
    Double2x2,
    Double2x3,
    Double2x4,
    Double3x2,
    Double3x3,
    Double3x4,
    Double4x2,
    Double4x3,
    Double4x4,
};

enum class VarType
{
    Undefined,
    Void,

    // Scalar types
    Bool,
    Int,
    UInt,
    Half,
    Float,
    Double,

    // Vector types
    Bool2,
    Bool3,
    Bool4,
    Int2,
    Int3,
    Int4,
    UInt2,
    UInt3,
    UInt4,
    Half2,
    Half3,
    Half4,
    Float2,
    Float3,
    Float4,
    Double2,
    Double3,
    Double4,

    // Matrix types
    Bool2x2,
    Bool2x3,
    Bool2x4,
    Bool3x2,
    Bool3x3,
    Bool3x4,
    Bool4x2,
    Bool4x3,
    Bool4x4,
    Int2x2,
    Int2x3,
    Int2x4,
    Int3x2,
    Int3x3,
    Int3x4,
    Int4x2,
    Int4x3,
    Int4x4,
    UInt2x2,
    UInt2x3,
    UInt2x4,
    UInt3x2,
    UInt3x3,
    UInt3x4,
    UInt4x2,
    UInt4x3,
    UInt4x4,
    Half2x2,
    Half2x3,
    Half2x4,
    Half3x2,
    Half3x3,
    Half3x4,
    Half4x2,
    Half4x3,
    Half4x4,
    Float2x2,
    Float2x3,
    Float2x4,
    Float3x2,
    Float3x3,
    Float3x4,
    Float4x2,
    Float4x3,
    Float4x4,
    Double2x2,
    Double2x3,
    Double2x4,
    Double3x2,
    Double3x3,
    Double3x4,
    Double4x2,
    Double4x3,
    Double4x4,
};

union DefaultValue
{
    bool boolean;
    float real;
    int integer;
    int imatrix[4];
    float matrix[16];
    int handle;
};

//! Information about a variable declaration
struct Variable
{
    //! Identifier of the element.
    std::string ident;

    //! Data type of the element.
    VariableType type = VariableType::Variable;

    //! Determines actual type of the element. Contents depend on "type".
    int baseType = 0;

    //! Number of elements in the array, if the variable is an array. Flattened if multi-dimensional array.
    int arraySize = 1;
};

//! A single element in a constant buffer or an opaque type
struct Uniform : Variable
{
    enum Flags
    {
        None        = 0,

        Internal    = 1 << 0,
        Color       = 1 << 1
    };

    //! Index of the uniform block this uniform belongs to. -1 if none.
    int uniformBlock = -1;

    //! Index into the default value array. -1 if no default value.
    int defaultValue = -1;

    //! Flags further defining the uniform.
    int flags = None;

    //! In case the parameter is used as a destination for sprite animation UVs, identifier of the texture its animating
    std::string spriteUVRef;
};

//! Information about a struct type
struct Struct
{
    std::string name;
    std::vector<Variable> members;
};

//! Single parameter in a function.
struct Parameter
{
    enum Flags
    {
        In = 1 << 0,
        Out = 1 << 1
    };

    VarType type;
    std::string ident;
    int flags;
};

//! A single function defined in the program
struct Function
{
    //! Name of the function
    std::string ident;

    //! Return value of the function
    VarType returnValue;

    //! List of all function parameters
    std::vector<Parameter> parameters;
};

// END BANSHEE CHANGES

//! Number of threads within each work group of a compute shader.
struct NumThreads
{
    //! Number of shader compute threads in X dimension.
    int x = 0;

    //! Number of shader compute threads in Y dimension.
    int y = 0;

    //! Number of shader compute threads in Z dimension.
    int z = 0;
};

//! Structure for shader output statistics (e.g. texture/buffer binding points).
struct ReflectionData
{
    //! All defined macros after pre-processing.
    std::vector<std::string>            macros;

    //! Texture bindings.
    std::vector<BindingSlot>            textures;

    //! Storage buffer bindings.
    std::vector<BindingSlot>            storageBuffers;

    //! Constant buffer bindings.
    std::vector<BindingSlot>            constantBuffers;

    //! Shader input attributes.
    std::vector<BindingSlot>            inputAttributes;

    //! Shader output attributes.
    std::vector<BindingSlot>            outputAttributes;

    //! Static sampler states (identifier, states).
    std::map<std::string, SamplerState> samplerStates;

    //! Non-programmable state that controls blending.
    BlendState                          blendState;

    //! Non-programmable state that controls rasterization.
    RasterizerState                     rasterizerState;

    //! Non-programmable state that controls depth buffer operations.
    DepthState                          depthState;

    //! Non-programmable state that controls stencil buffer operations.
    StencilState                        stencilState;

    //! Global options applied to all programs.
    GlobalOptions                       globalOptions;

    //! 'numthreads' attribute of a compute shader.
    NumThreads                          numThreads;

    // BEGIN BANSHEE CHANGES
    std::vector<Uniform>                uniforms;
    std::vector<Struct>                 structs;
    std::vector<DefaultValue>           defaultValues;

    std::vector<Function>               functions;
    // END BANSHEE CHANGES
};


} // /namespace Reflection


/* ===== Public functions ===== */

//! Returns the string representation of the specified 'SamplerState::Filter' type.
XSC_EXPORT std::string ToString(const Reflection::Filter t);

//! Returns the string representation of the specified 'SamplerState::TextureAddressMode' type.
XSC_EXPORT std::string ToString(const Reflection::TextureAddressMode t);

//! Returns the string representation of the specified 'ComparisonFunc' type.
XSC_EXPORT std::string ToString(const Reflection::ComparisonFunc t);

//! Returns the string representation of the specified 'BlendOpType' type.
XSC_EXPORT std::string ToString(const Reflection::BlendOpType t);

//! Returns the string representation of the specified 'StencilOpType' type.
XSC_EXPORT std::string ToString(const Reflection::StencilOpType t);

//! Returns the string representation of the specified 'FillMode' type.
XSC_EXPORT std::string ToString(const Reflection::FillMode t);

//! Returns the string representation of the specified 'CullMode' type.
XSC_EXPORT std::string ToString(const Reflection::CullMode t);

//! Returns the string representation of the specified 'BlendFactor' type.
XSC_EXPORT std::string ToString(const Reflection::BlendFactor t);

//! Returns the string representation of the specified 'SortMode' type.
XSC_EXPORT std::string ToString(const Reflection::SortMode t);

//! Prints the reflection data into the output stream in a human readable format.
XSC_EXPORT void PrintReflection(std::ostream& stream, const Reflection::ReflectionData& reflectionData);


} // /namespace Xsc


#endif



// ================================================================================