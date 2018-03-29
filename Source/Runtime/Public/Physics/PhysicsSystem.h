// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/*
-------------------------------------------------------------------------------

    Physics System

-------------------------------------------------------------------------------
*/

BE_NAMESPACE_BEGIN

class PhysicsSystem {
    friend class PhysCollidable;

public:
    void                    Init();
    void                    Shutdown();

    PhysicsWorld *          AllocPhysicsWorld();
    void                    FreePhysicsWorld(PhysicsWorld *renderWorld);
    
                            /// Create a collidable object with the given description.
    PhysCollidable *        CreateCollidable(const PhysCollidableDesc *desc);
                            /// Destroy a collidable object.
    void                    DestroyCollidable(PhysCollidable *collidable);
    
                            /// Create a constraint object with the given description.
    PhysConstraint *        CreateConstraint(const PhysConstraintDesc *desc);
                            /// Destroy a constraint object.
    void                    DestroyConstraint(PhysConstraint *constraint);

    void                    CheckModifiedCVars();

private:
    Array<PhysicsWorld *>   physicsWorlds;
};

extern PhysicsSystem        physicsSystem;

BE_NAMESPACE_END