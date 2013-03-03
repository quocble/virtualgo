/*
    Virtual Go GDC 2013 Demo
    Copyright (c) 2005-2013, Glenn Fiedler. All rights reserved.
*/

#include "Common.h"
#include "Render.h"
#include "Stone.h"
#include "Platform.h"
#include "Biconvex.h"
#include "RigidBody.h"
#include "CollisionDetection.h"
#include "CollisionResponse.h"
#include "Intersection.h"

using namespace platform;

const int MaxZoomLevel = 4;

int zoomLevel = 1;

const float ScrollSpeed = 10.0f;

float scrollX = 0;
float scrollY = 0;
float scrollZ = 0;

enum Mode
{
    Nothing,
    LinearCollisionResponse,
    AngularCollisionResponse,
    CollisionResponseWithFriction,
    NumModes
};

Mode mode = Nothing;

Mode cameraMode = mode;
vec3f cameraLookAt;
vec3f cameraPosition;

const float DefaultBoardThickness = 0.5f;

Board board( 35, 35, DefaultBoardThickness );

Stone stone;
StoneSize size = STONE_SIZE_34;
Stone stoneSizes[STONE_SIZE_NumValues];

void RandomStone( const Biconvex & biconvex, RigidBody & rigidBody, Mode mode )
{
    const float x = scrollX;
    const float z = scrollZ;
    rigidBody.position = vec3f( x, 15.0f, z );
    rigidBody.orientation = quat4f::axisRotation( random_float(0,2*pi), vec3f( random_float(0.1f,1), random_float(0.1f,1), random_float(0.1f,1) ) );
    if ( mode == LinearCollisionResponse )
        rigidBody.orientation = quat4f(1,0,0,0);
    rigidBody.linearMomentum = vec3f(0,0,0);
    if ( mode < CollisionResponseWithFriction )
        rigidBody.angularMomentum = vec3f(0,0,0);
    rigidBody.Update();
}

void SelectStoneSize( int newStoneSize )
{
    size = (StoneSize) clamp( newStoneSize, 0, STONE_SIZE_NumValues - 1 );
    stone.biconvex = stoneSizes[size].biconvex;
    stone.rigidBody.mass = stoneSizes[size].rigidBody.mass;
    stone.rigidBody.inverseMass = stoneSizes[size].rigidBody.inverseMass;
    stone.rigidBody.inertia = stoneSizes[size].rigidBody.inertia;
    stone.rigidBody.inertiaTensor = stoneSizes[size].rigidBody.inertiaTensor;
    stone.rigidBody.inverseInertiaTensor = stoneSizes[size].rigidBody.inverseInertiaTensor;
}

void RestoreDefaults()
{
    SelectStoneSize( STONE_SIZE_34 );
    board.SetThickness( DefaultBoardThickness );
    scrollX = 0;
    scrollY = 0;
    scrollZ = 0;
    zoomLevel = 1;
    RandomStone( stone.biconvex, stone.rigidBody, mode );
}

int main()
{
    printf( "[virtual go]\n" );

    for ( int i = 0; i < STONE_SIZE_NumValues; ++i )
        stoneSizes[i].Initialize( (StoneSize)i );

    SelectStoneSize( STONE_SIZE_34 );

    RandomStone( stone.biconvex, stone.rigidBody, mode );
    
    Mesh mesh[STONE_SIZE_NumValues];
    for ( int i = 0; i < STONE_SIZE_NumValues; ++i )
        GenerateBiconvexMesh( mesh[i], stoneSizes[i].biconvex );

    int displayWidth, displayHeight;
    GetDisplayResolution( displayWidth, displayHeight );

    #ifdef LETTERBOX
    displayWidth = 1280;
    displayHeight = 800;
    #endif

    printf( "display resolution is %d x %d\n", displayWidth, displayHeight );

    if ( !OpenDisplay( "Virtual Go", displayWidth, displayHeight, 32 ) )
    {
        printf( "error: failed to open display" );
        return 1;
    }

    HideMouseCursor();

    glEnable( GL_LINE_SMOOTH );
    glEnable( GL_POLYGON_SMOOTH );
    glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
    glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );

    glShadeModel( GL_SMOOTH );

    GLfloat lightAmbientColor[] = { 0.85, 0.85, 0.85, 1.0 };
    glLightModelfv( GL_LIGHT_MODEL_AMBIENT, lightAmbientColor );

    GLfloat lightPosition[] = { 0, 10, -10, 1 };
    glLightfv( GL_LIGHT0, GL_POSITION, lightPosition );

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );

    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LEQUAL );

    bool quit = false;

    srand( time( NULL ) );

    const float normal_dt = 1.0f / 60.0f;
    const float slowmo_dt = normal_dt * 0.1f;

    float dt = normal_dt;

    uint64_t frame = 0;

    bool prevSpace = false;
    bool prevEnter = false;
    bool prevCtrlUp = false;
    bool prevCtrlDown = false;
    bool slowmo = false;

    while ( !quit )
    {
        UpdateEvents();

        platform::Input input;
        
        input = platform::Input::Sample();

        if ( input.quit )
            quit = true;

        if ( input.escape )
            RestoreDefaults();

        if ( input.space && !prevSpace )
        {
            slowmo = !slowmo;
        }
        prevSpace = input.space;

        if ( input.enter && !prevEnter )
        {
            RandomStone( stone.biconvex, stone.rigidBody, mode );
            dt = normal_dt;
            slowmo = false;
        }
        prevEnter = input.enter;

        if ( input.tab )
        {
            scrollX += ( stone.rigidBody.position.x() - scrollX ) * 0.075f;
            scrollY += ( stone.rigidBody.position.y() - scrollY ) * 0.075f;
            scrollZ += ( stone.rigidBody.position.z() - scrollZ ) * 0.075f;
        }

        if ( input.alt )
        {
            if ( input.left )
                SelectStoneSize( size - 1 );
            
            if ( input.right )
                SelectStoneSize( size + 1 );

            if ( input.down )
            {
                float thickness = board.GetThickness();
                thickness *= 0.75f;
                if ( thickness < 0.5f )
                    thickness = 0.5f;
                board.SetThickness( thickness );
            }

            if ( input.up )
            {
                float thickness = board.GetThickness();
                thickness *= 1.25f;
                if ( thickness > 10.0f )
                    thickness = 10.0f;
                board.SetThickness( thickness );
            }
        }
        else if ( input.control )
        {
            if ( input.up && !prevCtrlUp )
            {
                zoomLevel --;
                if ( zoomLevel < 0 )
                    zoomLevel = 0;
            }
            prevCtrlUp = input.up;

            if ( input.down && !prevCtrlDown )
            {
                zoomLevel ++;
                if ( zoomLevel > MaxZoomLevel - 1 )
                    zoomLevel = MaxZoomLevel - 1;
            }
            prevCtrlDown = input.down;
        }
        else
        {
            float sx = 0;
            float sy = 0;
            float sz = 0;

            if ( input.left )
                sx = 1;

            if ( input.right )
                sx = -1;

            if ( input.up )
                sz = -1;

            if ( input.down )
                sz = 1;

            if ( input.a )
                sy = 1;

            if ( input.z )
                sy = -1;

            vec3f scroll(sx,sy,sz);

            if ( length_squared( scroll ) > 0 )
            {
                scroll = normalize( scroll ) * ScrollSpeed * normal_dt;
                scrollX += scroll.x();
                scrollY += scroll.y();
                scrollZ += scroll.z();
            }

            if ( input.one )
            {
                mode = LinearCollisionResponse;
                RandomStone( stone.biconvex, stone.rigidBody, mode );
                dt = normal_dt;
                slowmo = false;
            }

            if ( input.two )
            {
                mode = AngularCollisionResponse;
                RandomStone( stone.biconvex, stone.rigidBody, mode );
                dt = normal_dt;
                slowmo = false;
            }

            if ( input.three )
            {
                mode = CollisionResponseWithFriction;
                RandomStone( stone.biconvex, stone.rigidBody, mode );
                dt = normal_dt;
                slowmo = false;
            }

            prevCtrlUp = false;
            prevCtrlDown = false;
        }

        ClearScreen( displayWidth, displayHeight );

        if ( mode == Nothing )
        {
            UpdateDisplay( 1 );
            frame++;
            continue;
        }

        const float fov = 40.0f;

        glMatrixMode( GL_PROJECTION );

        glLoadIdentity();
        
        float flipX[] = { -1,0,0,0,
                           0,1,0,0,
                           0,0,1,0,
                           0,0,0,1 };
        
        glMultMatrixf( flipX );

        gluPerspective( fov, (float) displayWidth / (float) displayHeight, 0.1f, 100.0f );

        glMatrixMode( GL_MODELVIEW );    

        glLoadIdentity();

        // update camera

        const float x = scrollX;
        const float y = scrollY;
        const float z = scrollZ;

        vec3f targetLookAt;
        vec3f targetPosition;

        const float t = board.GetThickness();

        if ( zoomLevel == 0 )
        {
            targetLookAt = vec3f(x,y,z);
            targetPosition = vec3f(x,y,z+10);
        }
        else if ( zoomLevel == 1 )
        {
            targetLookAt = vec3f(x,y+1,z);
            targetPosition = vec3f(x,y+4,z+20);
        }
        else if ( zoomLevel == 2 )
        {
            targetLookAt = vec3f(x,y+2,z);
            targetPosition = vec3f(x,y+19,z+30);
        }
        else if ( zoomLevel == 3 )
        {
            targetLookAt = vec3f(x,y,z);
            targetPosition = vec3f(x,y+49,z);
        }

        if ( cameraMode == mode )
        {
            cameraLookAt += ( targetLookAt - cameraLookAt ) * 0.25f;
            cameraPosition += ( targetPosition - cameraPosition ) * 0.25f;
        }
        else
        {
            cameraLookAt = targetLookAt;
            cameraPosition = targetPosition;
        }

        float amountBelow = t - fmax( targetPosition.y(), targetLookAt.y() );
        if ( amountBelow > 0 )
            scrollY += amountBelow;

        cameraMode = mode;

        vec3f cameraUp = cross( normalize( cameraLookAt - cameraPosition ), vec3f(-1,0,0) );

        gluLookAt( cameraPosition.x(), max( cameraPosition.y(), t ), cameraPosition.z(),
                   cameraLookAt.x(), max( cameraLookAt.y(), t ), cameraLookAt.z(),
                   cameraUp.x(), cameraUp.y(), cameraUp.z() );

        // update stone physics

        const float target_dt = slowmo ? slowmo_dt : normal_dt;
        const float tightness = ( target_dt < dt ) ? 0.2f : 0.1f;
        dt += ( target_dt - dt ) * tightness;

        const int iterations = 20;

        const float iteration_dt = dt / iterations;

        for ( int i = 0; i < iterations; ++i )
        {
            bool colliding = false;

            const float gravity = 9.8f * 10;    // cms/sec^2
            stone.rigidBody.linearMomentum += vec3f(0,-gravity,0) * stone.rigidBody.mass * iteration_dt;

            stone.rigidBody.Update();

            stone.rigidBody.position += stone.rigidBody.linearVelocity * iteration_dt;
            quat4f spin = AngularVelocityToSpin( stone.rigidBody.orientation, stone.rigidBody.angularVelocity );
            stone.rigidBody.orientation += spin * iteration_dt;
            stone.rigidBody.orientation = normalize( stone.rigidBody.orientation );

            // collision between stone and board

            const float e = 0.85f;
            const float u = 0.15f;

            StaticContact boardContact;
            if ( StoneBoardCollision( stone.biconvex, board, stone.rigidBody, boardContact ) )
            {
                if ( mode == LinearCollisionResponse )
                    ApplyLinearCollisionImpulse( boardContact, e );
                else if ( mode == AngularCollisionResponse )
                    ApplyCollisionImpulseWithFriction( boardContact, e, 0.0f );
                else if ( mode >= CollisionResponseWithFriction )
                    ApplyCollisionImpulseWithFriction( boardContact, e, u );
                stone.rigidBody.Update();
            }

            // collision between stone and floor

            StaticContact floorContact;
            if ( StoneFloorCollision( stone.biconvex, board, stone.rigidBody, floorContact ) )
            {
                if ( mode == LinearCollisionResponse )
                    ApplyLinearCollisionImpulse( floorContact, e );
                else if ( mode == AngularCollisionResponse )
                    ApplyCollisionImpulseWithFriction( floorContact, e, 0.0f );
                else if ( mode >= CollisionResponseWithFriction )
                    ApplyCollisionImpulseWithFriction( floorContact, e, u );
                stone.rigidBody.Update();
            }
        }

        // render board

        glColor4f( 0,0,0,1 );
        glDisable( GL_LIGHTING );
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        RenderBoard( board );

        glLineWidth( 5 );
        glDisable( GL_DEPTH_TEST );
        glEnable( GL_LIGHTING );
        glColor4f( 0.8f,0.8f,0.8f,1 );
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
        RenderBoard( board );
        glEnable( GL_DEPTH_TEST );

        // render stone

        glPushMatrix();

        glDepthMask( GL_FALSE );

        RigidBodyTransform biconvexTransform( stone.rigidBody.position, stone.rigidBody.orientation );
        float opengl_transform[16];
        biconvexTransform.localToWorld.store( opengl_transform );
        glMultMatrixf( opengl_transform );

        glEnable( GL_BLEND ); 
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glLineWidth( 1 );
        glColor4f( 1, 1, 1, 1 );
        RenderMesh( mesh[size] );

        glDepthMask( GL_TRUE );

        glPopMatrix();

        // update the display
        
        UpdateDisplay( 1 );

        // update time

        frame++;
    }

    CloseDisplay();

    return 0;
}
