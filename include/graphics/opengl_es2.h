#ifndef _OPEN_GL_ES2_H_
#define _OPEN_GL_ES2_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>
#include<stdbool.h>

typedef uint32_t GPU_HANDLE;
typedef uint32_t VC4_ADDR;


/*--------------------------------------------------------------------------}
;{	    DEFINE A RENDER STRUCTURE ... WHICH JUST HOLD RENDER DETAILS	  	}
;{-------------------------------------------------------------------------*/
typedef struct render_t
{
	/* This is the current load position */
	VC4_ADDR loadpos;							// Physical load address as ARM address

	/* These are all the same thing just handle and two different address GPU and ARM */
	GPU_HANDLE rendererHandle;					// Renderer memory handle
	VC4_ADDR rendererDataVC4;					// Renderer data VC4 locked address
	
	uint16_t renderWth;							// Render width
	uint16_t renderHt;							// Render height

	VC4_ADDR shaderStart;						// VC4 address shader code starts 
	VC4_ADDR fragShaderRecStart;				// VC4 start address for fragment shader record

	uint32_t binWth;							// Bin width
	uint32_t binHt;								// Bin height

	VC4_ADDR renderControlVC4;					// VC4 render control start address
	VC4_ADDR renderControlEndVC4;				// VC4 render control end address

	VC4_ADDR vertexVC4;							// VC4 address to vertex data
	uint32_t num_verts;							// number of vertices 

	VC4_ADDR indexVertexVC4;					// VC4 address to start of index vertex data
	uint32_t IndexVertexCt;						// Index vertex count
	uint32_t MaxIndexVertex;					// Maximum Index vertex referenced

	/* TILE DATA MEMORY ... HAS TO BE 4K ALIGN */
	GPU_HANDLE tileHandle;						// Tile memory handle
	uint32_t  tileMemSize;						// Tiel memory size;
	VC4_ADDR tileStateDataVC4;					// Tile data VC4 locked address
	VC4_ADDR tileDataBufferVC4;					// Tile data buffer VC4 locked address

	/* BINNING DATA MEMORY ... HAS TO BE 4K ALIGN */
	GPU_HANDLE binningHandle;					// Binning memory handle
	VC4_ADDR binningDataVC4;					// Binning data VC4 locked address
	VC4_ADDR binningCfgEnd;						// VC4 binning config end address

} RENDER_STRUCT;


bool v3d_InitializeScene (RENDER_STRUCT* scene, uint32_t renderWth, uint32_t renderHt);
bool v3d_AddVertexesToScene (RENDER_STRUCT* scene);
bool v3d_AddShadderToScene (RENDER_STRUCT* scene, uint32_t* frag_shader, uint32_t frag_shader_emits);
bool v3d_SetupRenderControl(RENDER_STRUCT* scene, VC4_ADDR renderBufferAddr);
bool v3d_SetupBinningConfig (RENDER_STRUCT* scene);
void v3d_RenderScene (RENDER_STRUCT* scene);

#ifdef __cplusplus
}
#endif

#endif