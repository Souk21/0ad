#include "precompiled.h"

#include "gui/MiniMap.h"
#include "ps/Game.h"
#include "CConsole.h"

#include "ogl.h"
#include "renderer/Renderer.h"
#include "graphics/TextureEntry.h"
#include "graphics/TextureManager.h"
#include "graphics/Unit.h"
#include "graphics/Camera.h"


#include "Bound.h"
#include "Model.h"

extern CConsole* g_Console;
extern int g_mouse_x, g_mouse_y;

static unsigned int ScaleColor(unsigned int color,float x)
{
    unsigned int r=uint(float(color & 0xff)*x);
    unsigned int g=uint(float((color>>8) & 0xff)*x);
    unsigned int b=uint(float((color>>16) & 0xff)*x);
    return (0xff000000 | r | g<<8 | b<<16);
}

static int RoundUpToPowerOf2(int x)
{
    if ((x & (x-1))==0) return x;
    int d=x;
    while (d & (d-1)) {
        d&=(d-1);
    }
    return d<<1;
}

CMiniMap::CMiniMap()
    : m_Handle(0), m_Data(NULL), m_MapSize(0), m_Terrain(0),
    m_UnitManager(0)
{
    AddSetting(GUIST_CColor,	"fov_wedge_color");
	AddSetting(GUIST_CStr,		"tooltip");
	AddSetting(GUIST_CStr,		"tooltip_style");
}

CMiniMap::~CMiniMap()
{
    Destroy();
}

void CMiniMap::Draw()
{
    if(GetGUI() && g_Game)
    {
        if(!m_Handle)
            GenerateMiniMapTexture();

        if(!m_Terrain || !m_UnitManager)
            return;

        g_Renderer.BindTexture(0, m_Handle);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        float texCoordMax = ((float)m_MapSize - 1) / ((float)m_TextureSize);

        glBegin(GL_QUADS);

        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(m_CachedActualSize.left, m_CachedActualSize.top, GetBufferedZ());
        glTexCoord2f(texCoordMax, 0.0f);
        glVertex3f(m_CachedActualSize.right, m_CachedActualSize.top, GetBufferedZ());
        glTexCoord2f(texCoordMax, texCoordMax);
        glVertex3f(m_CachedActualSize.right, m_CachedActualSize.bottom, GetBufferedZ());
        glTexCoord2f(0.0f, texCoordMax);
        glVertex3f(m_CachedActualSize.left, m_CachedActualSize.bottom, GetBufferedZ());

        glEnd();

        float x = m_CachedActualSize.left;
        float y = m_CachedActualSize.top;
        std::vector<CUnit *> units = m_UnitManager->GetUnits();
        std::vector<CUnit *>::iterator iter = units.begin();
        CUnit *unit = NULL;
        CVector2D pos;

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_POINT_SMOOTH);
        glDisable(GL_TEXTURE_2D);
        glPointSize(3.0f);
		// REMOVED: glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_POINTS);
        for(; iter != units.end(); iter++)
        {
            unit = (CUnit *)(*iter);
            if(unit && unit->GetEntity())
            {
				// EDIT: John M. Mena // Set the player colour
				const SPlayerColour& colour = unit->GetEntity()->GetPlayer()->GetColour();
				glColor3f(colour.r, colour.g, colour.b);

                pos = GetMapSpaceCoords(unit->GetEntity()->m_position);

				// TODO: Investigate why player position must be reversed with map.
				// EDIT: John M. Mena // Reversed x and y addition
									  // Not quite sure what the problems is here.
                glVertex3f(x + pos.y, y + pos.x, GetBufferedZ());
            }
        }

        glEnd();


		// render view rect : John M. Mena
		CCamera &g_Camera=*g_Game->GetView()->GetCamera();
		CVector3D pos3D = g_Camera.GetWorldCoordinates();
		pos = GetMapSpaceCoords(pos3D);

		// Restrict the drawing to the map
		glScissor((int)m_CachedActualSize.left, 0, (int)m_CachedActualSize.right, (int)m_CachedActualSize.GetHeight());
		glEnable(GL_SCISSOR_TEST);

		glLineWidth(2);
		glColor3f(1.0f,0.3f,0.3f);

		// Draw the viewing rectangle
		glBegin(GL_LINE_LOOP);
		glVertex2f(x + pos.y - 10, y + pos.x - 10);
		glVertex2f(x + pos.y + 10, y + pos.x - 10);
		glVertex2f(x + pos.y + 10, y + pos.x + 10);
		glVertex2f(x + pos.y - 10, y + pos.x + 10);
		glEnd();

		glDisable(GL_SCISSOR_TEST);

        glPointSize(1.0f);
		glLineWidth(1.0f);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_POINT_SMOOTH);
        glEnable(GL_DEPTH_TEST);

        /*glLineWidth(2);
        glColor3f(0.4f,0.35f,0.8f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(m_CachedActualSize.left, m_CachedActualSize.top, GetBufferedZ());
        glVertex3f(m_CachedActualSize.right, m_CachedActualSize.top, GetBufferedZ());
        glVertex3f(m_CachedActualSize.right, m_CachedActualSize.bottom, GetBufferedZ());
        glVertex3f(m_CachedActualSize.left, m_CachedActualSize.bottom, GetBufferedZ());
        glEnd();*/
    }
}

void CMiniMap::GenerateMiniMapTexture()
{
    m_Terrain = g_Game->GetWorld()->GetTerrain();
    m_UnitManager = g_Game->GetWorld()->GetUnitManager();
    if(!m_Terrain)
        return;

    m_Width = (u32)(m_CachedActualSize.right - m_CachedActualSize.left);
    m_Height = (u32)(m_CachedActualSize.bottom - m_CachedActualSize.top);

    Destroy();

    glGenTextures(1, (GLuint *)&m_Handle);
    g_Renderer.BindTexture(0, m_Handle);
    
    m_MapSize = m_Terrain->GetVerticesPerSide();
    m_TextureSize = RoundUpToPowerOf2(m_MapSize);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_TextureSize, m_TextureSize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

    m_Data = new u32[(m_MapSize - 1) * (m_MapSize - 1)];
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);

    Rebuild();
}

void CMiniMap::Rebuild()
{
    u32 mapSize = m_Terrain->GetVerticesPerSide();
    u32 x = 0;
    u32 y = 0;
    u32 w = m_MapSize - 1;
    u32 h = m_MapSize - 1;

    for(u32 j = 0; j < h; j++)
    {
        u32 *dataPtr = m_Data + ((y + j) * (mapSize - 1)) + x;
        for(u32 i = 0; i < w; i++)
        {
            int hmap = ((int)m_Terrain->GetHeightMap()[(y + j) * mapSize + x + i]) >> 8;
            int val = (hmap / 3) + 170;
            CMiniPatch *mp = m_Terrain->GetTile(x + i, y + j);
            u32 color = 0;
            if(mp)
            {
                CTextureEntry *tex = mp->Tex1 ? g_TexMan.FindTexture(mp->Tex1) : 0;
                color = tex ? tex->GetBaseColor() : 0xffffffff;
            }
            else
                color = 0xffffffff;

            *dataPtr++ = ScaleColor(color, ((float)val) / 255.0f);
        }
    }

    UploadTexture();
}

void CMiniMap::UploadTexture()
{
    g_Renderer.BindTexture(0, m_Handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_MapSize - 1, m_MapSize - 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, m_Data);
}

void CMiniMap::Destroy()
{
    if(m_Handle)
        glDeleteTextures(1, (GLuint *)&m_Handle);

    if(m_Data)
    {
        delete[] m_Data;
        m_Data = NULL;
    }
}

/*
 * Calefaction
 * TODO: Speed this up. There has to be some mathematical way to make
 * this more efficient. This works for now.
 */
CVector2D CMiniMap::GetMapSpaceCoords(CVector3D worldPos)
{
    u32 x = (u32)(worldPos.X / CELL_SIZE);
    // Entity's Z coordinate is really its longitudinal coordinate on the terrain
    u32 y = (u32)(worldPos.Z / CELL_SIZE);

    // Calculate map space scale
    float scaleX = float(m_Width) / float(m_MapSize - 1);
    float scaleY = float(m_Height) / float(m_MapSize - 1);
    return CVector2D(float(x) * scaleX, float(y) * scaleY);
}
