/*
 *  ofxFluid.cpp
 *
 *  Created by Patricio Gonzalez Vivo on 9/29/11.
 *  Copyright 2011 http://PatricioGonzalezVivo.com All rights reserved.
 *
 *  Created ussing:
 *
 *    - Mark Harris article from GPU Gems 1
 *      http://http.developer.nvidia.com/GPUGems/gpugems_ch38.html
 *
 *    - Phil Rideout
 *      http://prideout.net/blog/?p=58
 *  
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 *  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 *  OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  ************************************************************************************ */

#include "ofxFluid.h"

ofxFluid::ofxFluid(){
    passes = 1;
    internalFormat = GL_RGBA;

	bool isGL3 = ofIsGLProgrammableRenderer();

	// define vertex shader for programable renderer
	if (isGL3) {
		vertexShader = "#version 150\n";
		vertexShader += STRINGIFY(
			uniform mat4 modelViewProjectionMatrix;
		in vec4 position;
		in vec2 texcoord;
		out vec2 vtexcoord;
		void main() {
			vtexcoord = texcoord;
			gl_Position = modelViewProjectionMatrix * position;
		});
	}
    
    // ADVECT
    fragmentShader = isGL3 ? "#version 150\n" : "";
	fragmentShader += STRINGIFY(uniform sampler2DRect tex0;         // Real obstacles
                               uniform sampler2DRect backbuffer;
                               uniform sampler2DRect VelocityTexture;
                               
                               uniform float TimeStep;
                               uniform float Dissipation;
							   );
	fragmentShader += isGL3 ?  "in vec2 vtexcoord; out vec4 color_out;" : "";
	
	fragmentShader +=		   "void main(){";
	
	fragmentShader += isGL3 ?		"vec2 st = vtexcoord;" : 
									"vec2 st = gl_TexCoord[0].st;";
	
	fragmentShader += STRINGIFY(    float solid = texture2DRect(tex0, st).r;
                                   
                                    if (solid > 0.1) {
								);
	fragmentShader += isGL3 ?		"color_out = vec4(0.0,0.0,0.0,0.0);" : 
									"gl_FragColor = vec4(0.0,0.0,0.0,0.0);";
	fragmentShader += STRINGIFY(
                                       return;
                                    }
                                   
                                    vec2 u = texture2DRect(VelocityTexture, st).rg;
                                    vec2 coord =  st - TimeStep * u;
								);
	fragmentShader += isGL3 ?	    "color_out = Dissipation * texture2DRect(backbuffer, coord);" : 
									"gl_FragColor = Dissipation * texture2DRect(backbuffer, coord);";
	fragmentShader +=			"}";
                           
    
    
    // JACOBI
    string fragmentJacobiShader = isGL3 ? "#version 150\n" : "";
	fragmentJacobiShader += STRINGIFY(uniform sampler2DRect Pressure;
                                    uniform sampler2DRect Divergence;
                                    uniform sampler2DRect tex0;
                                            
                                    uniform float Alpha;
                                    uniform float InverseBeta;
									);
	fragmentJacobiShader += isGL3 ?	"in vec2 vtexcoord; out vec4 color_out;" : "";
	
	fragmentJacobiShader +=			"void main() {";
	
	fragmentJacobiShader += isGL3 ?		"vec2 st = vtexcoord;" :
										"vec2 st = gl_TexCoord[0].st;";
	fragmentJacobiShader += STRINGIFY(
                                        vec4 pN = texture2DRect(Pressure, st + vec2(0.0, 1.0));
                                        vec4 pS = texture2DRect(Pressure, st + vec2(0.0, -1.0));
                                        vec4 pE = texture2DRect(Pressure, st + vec2(1.0, 0.0));
                                        vec4 pW = texture2DRect(Pressure, st + vec2(-1.0, 0.0));
                                        vec4 pC = texture2DRect(Pressure, st);
                                                
                                        vec3 oN = texture2DRect(tex0, st + vec2(0.0, 1.0)).rgb;
                                        vec3 oS = texture2DRect(tex0, st + vec2(0.0, -1.0)).rgb;
                                        vec3 oE = texture2DRect(tex0, st + vec2(1.0, 0.0)).rgb;
                                        vec3 oW = texture2DRect(tex0, st + vec2(-1.0, 0.0)).rgb;
                                                
                                        if (oN.x > 0.1) pN = pC;
                                        if (oS.x > 0.1) pS = pC;
                                        if (oE.x > 0.1) pE = pC;
                                        if (oW.x > 0.1) pW = pC;
                                                
                                        vec4 bC = texture2DRect(Divergence, st );
									 );
	fragmentJacobiShader += isGL3 ? "color_out = (pW + pE + pS + pN + Alpha * bC) * InverseBeta;" :
									"gl_FragColor = (pW + pE + pS + pN + Alpha * bC) * InverseBeta;";
	fragmentJacobiShader += "}";
                                    
    
    jacobiShader.unload();
	if (isGL3) jacobiShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    jacobiShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentJacobiShader);
	if (isGL3) jacobiShader.bindDefaults();
    jacobiShader.linkProgram();
    
    
    //SUBSTRACT GRADIENT
	string fragmentSubtractGradientShader = isGL3 ? "#version 150\n" : "";
    fragmentSubtractGradientShader += STRINGIFY(uniform sampler2DRect Velocity;
                                                uniform sampler2DRect Pressure;
                                                uniform sampler2DRect tex0;
                                                      
                                                uniform float GradientScale;
												);
	fragmentSubtractGradientShader += isGL3 ?   "in vec2 vtexcoord; out vec4 color_out;" : "";

	fragmentSubtractGradientShader +=           "void main() {";

	fragmentSubtractGradientShader += isGL3 ?		"vec2 st = vtexcoord;" :
													"vec2 st = gl_TexCoord[0].st;";
	fragmentSubtractGradientShader += STRINGIFY(                                                          
                                                    vec3 oC = texture2DRect(tex0, st ).rgb;
                                                    if (oC.x > 0.1) {
											    );
	fragmentSubtractGradientShader += isGL3 ?		    "color_out.gb = oC.yz;" : 
														"gl_FragColor.gb = oC.yz;";
	fragmentSubtractGradientShader += STRINGIFY(
                                                        return;
                                                    }
                                                          
                                                    float pN = texture2DRect(Pressure, st + vec2(0.0, 1.0)).r;
                                                    float pS = texture2DRect(Pressure, st + vec2(0.0, -1.0)).r;
                                                    float pE = texture2DRect(Pressure, st + vec2(1.0, 0.0)).r;
                                                    float pW = texture2DRect(Pressure, st + vec2(-1.0, 0.0)).r;
                                                    float pC = texture2DRect(Pressure, st).r;
                                                          
                                                    vec3 oN = texture2DRect(tex0, st + vec2(0.0, 1.0)).rgb;
                                                    vec3 oS = texture2DRect(tex0, st + vec2(0.0, -1.0)).rgb;
                                                    vec3 oE = texture2DRect(tex0, st + vec2(1.0, 0.0)).rgb;
                                                    vec3 oW = texture2DRect(tex0, st + vec2(-1.0, 0.0)).rgb;
                                                          
                                                    vec2 obstV = vec2(0.0,0.0);
                                                    vec2 vMask = vec2(1.0,1.0);
                                                          
                                                    if (oN.x > 0.1) { pN = pC; obstV.y = oN.z; vMask.y = 0.0; }\
                                                    if (oS.x > 0.1) { pS = pC; obstV.y = oS.z; vMask.y = 0.0; }\
                                                    if (oE.x > 0.1) { pE = pC; obstV.x = oE.y; vMask.x = 0.0; }\
                                                    if (oW.x > 0.1) { pW = pC; obstV.x = oW.y; vMask.x = 0.0; }\
                                                          
                                                    vec2 oldV = texture2DRect(Velocity, st).rg;
                                                    vec2 grad = vec2(pE - pW, pN - pS) * GradientScale;
                                                    vec2 newV = oldV - grad;
											   );
	fragmentSubtractGradientShader += isGL3 ?	    "color_out.rg = (vMask * newV) + obstV;" : 
													"gl_FragColor.rg = (vMask * newV) + obstV;";                                                          
	fragmentSubtractGradientShader +=		   "}";
                                                
    subtractGradientShader.unload();
	if (isGL3) subtractGradientShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    subtractGradientShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentSubtractGradientShader);
	if (isGL3) subtractGradientShader.bindDefaults();
	subtractGradientShader.linkProgram();
    
    
    // COMPUTE DIVERGENCE
	string fragmentComputeDivergenceShader = isGL3 ? "#version 150\n" : "";
    fragmentComputeDivergenceShader += STRINGIFY(uniform sampler2DRect Velocity;
                                                    uniform sampler2DRect tex0;
                                                       
                                                    uniform float HalfInverseCellSize;
													);
	fragmentComputeDivergenceShader += isGL3 ?		"in vec2 vtexcoord; out vec4 color_out;" : "";

	fragmentComputeDivergenceShader +=				"void main() {";

	fragmentComputeDivergenceShader += isGL3 ?			"vec2 st = vtexcoord;" :
														"vec2 st = gl_TexCoord[0].st;";
	fragmentComputeDivergenceShader += STRINGIFY(
                                                        vec2 vN = texture2DRect(Velocity, st + vec2(0.0,1.0)).rg;
                                                        vec2 vS = texture2DRect(Velocity, st + vec2(0.0,-1.0)).rg;
                                                        vec2 vE = texture2DRect(Velocity, st + vec2(1.0,0.0)).rg;
                                                        vec2 vW = texture2DRect(Velocity, st + vec2(-1.0,0.0)).rg;
                                                           
                                                        vec3 oN = texture2DRect(tex0, st + vec2(0.0,1.0)).rgb;
                                                        vec3 oS = texture2DRect(tex0, st + vec2(0.0,-1.0)).rgb;
                                                        vec3 oE = texture2DRect(tex0, st + vec2(1.0,0.0)).rgb;
                                                        vec3 oW = texture2DRect(tex0, st + vec2(-1.0,0.0)).rgb;
                                                           
                                                        if (oN.x > 0.1) vN = oN.yz;
                                                        if (oS.x > 0.1) vS = oS.yz;
                                                        if (oE.x > 0.1) vE = oE.yz;
                                                        if (oW.x > 0.1) vW = oW.yz;
												);
	fragmentComputeDivergenceShader += isGL3 ?			"color_out.r = HalfInverseCellSize * (vE.x - vW.x + vN.y - vS.y);" : 
														"gl_FragColor.r = HalfInverseCellSize * (vE.x - vW.x + vN.y - vS.y);";
	fragmentComputeDivergenceShader +=				"}";
                                                    
    computeDivergenceShader.unload();
	if (isGL3) computeDivergenceShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    computeDivergenceShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentComputeDivergenceShader);
	if (isGL3) computeDivergenceShader.bindDefaults();
	computeDivergenceShader.linkProgram();
    
    // APPLY TEXTURE
	string fragmentApplyTextureShader = isGL3 ? "#version 150\n" : "";
    fragmentApplyTextureShader += STRINGIFY(uniform sampler2DRect backbuffer;
                                            uniform sampler2DRect tex1;
                                            uniform float   pct;
                                            uniform int   isVel;
											);
	fragmentApplyTextureShader += isGL3 ?	"in vec2 vtexcoord; out vec4 color_out;" : "";

	fragmentApplyTextureShader +=			"void main() {";

	fragmentApplyTextureShader += isGL3 ?		"vec2 st = vtexcoord;" :
												"vec2 st = gl_TexCoord[0].st;";
	fragmentApplyTextureShader += STRINGIFY(
                                                vec4 prevFrame = texture2DRect(backbuffer, st);
                                                vec4 newFrame = texture2DRect(tex1, st);
                                                      
                                                if (isVel!=0){
                                                    newFrame -=0.5;
                                                    newFrame *=2.0;
                                                    newFrame.b = 0.5;
                                                }
                                                    
                                                //color_out = prevFrame+newFrame*pct;//mix(prevFrame,newFrame,pct);

												vec4 mixed = prevFrame + newFrame * pct;
												if (isVel == 0) {
													mixed.a = min(mixed.a, 1.0); //limit to 1.0
												}
											);
	fragmentApplyTextureShader += isGL3 ?		"color_out = mixed;" :
												"gl_FragColor = mixed;";
	fragmentApplyTextureShader +=			"}";
                                            
    applyTextureShader.unload();
	if (isGL3) applyTextureShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    applyTextureShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentApplyTextureShader);
	if (isGL3) applyTextureShader.bindDefaults();
	applyTextureShader.linkProgram();
    
    // APPLY IMPULSE
	string fragmentApplyImpulseShader = isGL3 ? "#version 150\n" : "";
	fragmentApplyImpulseShader += STRINGIFY(uniform vec2    Point;
                                            uniform float   Radius;
                                            uniform vec3    Value;
											);
	fragmentApplyImpulseShader += isGL3 ?	"in vec2 vtexcoord; out vec4 color_out;" : "";

	fragmentApplyImpulseShader +=			"void main() {";

	fragmentApplyImpulseShader += isGL3 ?		"vec2 st = vtexcoord;" :
												"vec2 st = gl_TexCoord[0].st;";
	fragmentApplyImpulseShader += STRINGIFY(
                                                float d = distance(Point, st);
                                                if (d < Radius) {
                                                    float a = (Radius - d) * 0.5;
                                                    a = min(a, 1.0);
											);
	if (isGL3) {
		fragmentApplyImpulseShader += STRINGIFY(
                                                    color_out = vec4(Value, a);
                                                } else {
                                                    color_out = vec4(0);
                                                }
                                            }
											);	
	}
	else {
		fragmentApplyImpulseShader += STRINGIFY(
													gl_FragColor = vec4(Value, a);
												} else {
													gl_FragColor = vec4(0);
												}
											}
											);
	}            
    
    applyImpulseShader.unload();
	if (isGL3) applyImpulseShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    applyImpulseShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentApplyImpulseShader);
	if (isGL3) applyImpulseShader.bindDefaults();
	applyImpulseShader.linkProgram();
    
    //APPLY BUOYANCY
    string fragmentApplyBuoyancyShader = isGL3 ? "#version 150\n" : "";
	fragmentApplyBuoyancyShader += STRINGIFY(uniform sampler2DRect Velocity;
                                            uniform sampler2DRect Temperature;
                                            uniform sampler2DRect Density;
                                                   
                                            uniform float AmbientTemperature;
                                            uniform float TimeStep;
                                            uniform float Sigma;
                                            uniform float Kappa;
                                                   
                                            uniform vec2  Gravity;
											);
	fragmentApplyBuoyancyShader += isGL3 ?	"in vec2 vtexcoord; out vec4 color_out;" : "";

	fragmentApplyBuoyancyShader +=			"void main() {";

	fragmentApplyBuoyancyShader += isGL3 ?		"vec2 st = vtexcoord;" :
												"vec2 st = gl_TexCoord[0].st;";
	fragmentApplyBuoyancyShader += STRINGIFY(
                                                float T = texture2DRect(Temperature, st).r;
                                                vec2 V = texture2DRect(Velocity, st).rg;
										   );
	fragmentApplyBuoyancyShader += isGL3 ?		"color_out.rg = V;" :
												"gl_FragColor.rg = V;";
	fragmentApplyBuoyancyShader += STRINGIFY(
                                                if (T > AmbientTemperature) {
                                                    float D = texture2DRect(Density, st).r;
											);
	fragmentApplyBuoyancyShader += isGL3 ?			"color_out.rg += (TimeStep * (T - AmbientTemperature) * Sigma - D * Kappa ) * Gravity;" : 
													"gl_FragColor.rg += (TimeStep * (T - AmbientTemperature) * Sigma - D * Kappa ) * Gravity;";
	fragmentApplyBuoyancyShader += STRINGIFY(
                                                }
                                            }
											);
                                            
    applyBuoyancyShader.unload();
	if (isGL3) applyBuoyancyShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
    applyBuoyancyShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentApplyBuoyancyShader);
	if (isGL3) applyBuoyancyShader.bindDefaults();
	applyBuoyancyShader.linkProgram();
    
    cellSize            = 1.25f; 
    gradientScale       = 1.00f / cellSize;
    ambientTemperature  = 0.0f;
    numJacobiIterations = 40;
    timeStep            = 0.125f;
    smokeBuoyancy       = 1.0f;
    smokeWeight         = 0.05f;
    
    bObstacles          = true;
    
    gForce.set(0,-0.98);
}

void ofxFluid::allocate(int _width, int _height, float _scale, bool _HD){
    width = _width; 
    height = _height; 
    scale = _scale;
    
    gridWidth = width * scale;
    gridHeight = height * scale;
    
    pingPong.allocate(gridWidth,gridHeight,(_HD)?GL_RGBA32F:GL_RGBA);
    dissipation = 0.999f;
    velocityBuffer.allocate(gridWidth,gridHeight,GL_RGB32F);
    velocityDissipation = 0.9f;
    temperatureBuffer.allocate(gridWidth,gridHeight,GL_RGB32F);
    temperatureDissipation = 0.99f;
    pressureBuffer.allocate(gridWidth,gridHeight,GL_RGB32F);
    pressureDissipation = 0.9f;
    
    initFbo(obstaclesFbo, gridWidth, gridHeight, GL_RGBA);
    initFbo(divergenceFbo, gridWidth, gridHeight, GL_RGB16F);
    
    compileCode();
    
    temperatureBuffer.src->begin();
    ofClear( ambientTemperature );
    temperatureBuffer.src->end();
    
    colorAddFbo.allocate(gridWidth,gridHeight,(_HD)?GL_RGBA32F:GL_RGBA);
    colorAddFbo.begin();
    ofClear(0,0);
    colorAddFbo.end();
    
    velocityAddFbo.allocate(gridWidth,gridHeight,GL_RGB32F);
    velocityAddFbo.begin();
    ofClear(0);
    velocityAddFbo.end();
    
    colorAddPct = 0.0;
    velocityAddPct = 0.0;
}

void ofxFluid::setUseObstacles(bool _do){
    bObstacles = _do;
    
    if(!bObstacles){
        obstaclesFbo.begin();
        ofClear(0,0);
        obstaclesFbo.end();
    }
}

void ofxFluid::addTemporalForce(ofPoint _pos, ofPoint _vel, ofFloatColor _col, float _rad, float _temp, float _den){
    punctualForce f;
    
    f.pos = _pos * scale;
    f.vel = _vel;
    f.color.set(_col.r,_col.g,_col.b);
    f.rad = _rad;
    f.temp = _temp;
    f.den = _den;

    temporalForces.push_back(f);
}

void ofxFluid::addConstantForce(ofPoint _pos, ofPoint _vel, ofFloatColor _col, float _rad, float _temp, float _den){
    punctualForce f;
    
    f.pos = _pos * scale;
    f.vel = _vel;
    f.color.set(_col.r,_col.g,_col.b);
    f.rad = _rad;
    f.temp = _temp;
    f.den = _den;
    
    constantForces.push_back(f);
}

void ofxFluid::setObstacles(ofBaseHasTexture &_tex){
    ofPushStyle();
    obstaclesFbo.begin();
    ofSetColor(255, 255);
    _tex.getTexture().draw(0,0,obstaclesFbo.getWidth(),obstaclesFbo.getHeight());
    obstaclesFbo.end();
    ofPopStyle();
}

void ofxFluid::addColor(ofBaseHasTexture &_tex, float _pct){
    addColor(_tex.getTexture(),_pct);
}

void ofxFluid::addColor(ofTexture &_tex, float _pct){
    ofPushStyle();
    colorAddFbo.begin();
    ofClear(0,0);
    ofSetColor(255);
    _tex.draw(0,0,gridWidth,gridHeight);
    colorAddFbo.end();
    ofPopStyle();
    
    colorAddPct = _pct;
}

void ofxFluid::addVelocity(ofTexture &_tex, float _pct){
    ofPushStyle();
    velocityAddFbo.begin();
    ofClear(0);
    ofSetColor(255, 255);
    _tex.draw(0,0,gridWidth,gridHeight);
    velocityAddFbo.end();
    ofPopStyle();
    
    velocityAddPct = _pct;
}

const ofTexture & ofxFluid::getTexture() const {
    return getTexture();
};


void ofxFluid::addVelocity(ofBaseHasTexture &_tex, float _pct){
    addVelocity(_tex.getTexture(),_pct);
}

void ofxFluid::clear(float _alpha){
    pingPong.clear();
    velocityBuffer.clear();
    temperatureBuffer.clear();
    pressureBuffer.clear();
    obstaclesFbo.begin();
    ofClear(0);
    obstaclesFbo.end();
    divergenceFbo.begin();
    ofClear(0);
    divergenceFbo.end();
    temperatureBuffer.src->begin();
    ofClear(ambientTemperature);
    temperatureBuffer.src->end();
    colorAddFbo.begin();
    ofClear(0,_alpha*255);
    colorAddFbo.end();
    velocityAddFbo.begin();
    ofClear(0);
    velocityAddFbo.end();
}

void ofxFluid::update(){
    ofDisableAlphaBlending();
    
    //  Scale Obstacles
    //
    if(bObstacles && bUpdate){
        ofPushStyle();
        obstaclesFbo.begin();
        ofSetColor(255, 255);
        textures[0].draw(0,0,gridWidth,gridHeight);
        obstaclesFbo.end();
        ofPopStyle();
    }
    
    //  Pre-Compute
    //
    advect(velocityBuffer, velocityDissipation);
    velocityBuffer.swap();
    
    advect(temperatureBuffer, temperatureDissipation);
    temperatureBuffer.swap();
    
    advect(pingPong, dissipation);
    pingPong.swap();
    
    applyBuoyancy();
    velocityBuffer.swap();
    
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofDisableBlendMode();
    //  Update temperature, Color (PingPong) and Velocity buffers
    //
    if(colorAddPct>0.0||velocityAddPct>0.0){
        applyImpulse(temperatureBuffer, colorAddFbo, colorAddPct);
        applyImpulse(velocityBuffer, velocityAddFbo, velocityAddPct, true);
    }
    
    if(colorAddPct>0.0){
        applyImpulse(pingPong, colorAddFbo, colorAddPct);
        colorAddFbo.begin();
        ofClear(0,0);
        colorAddFbo.end();
        colorAddPct = 0.0;
    }
    
    if(velocityAddPct>0.0){
        applyImpulse(velocityBuffer, velocityAddFbo, velocityAddPct, true);
        velocityAddFbo.begin();
        ofClear(0);
        velocityAddFbo.end();
        velocityAddPct = 0.0;
    }
    
    if ( temporalForces.size() != 0){
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofDisableBlendMode();
        
        for(int i = 0; i < temporalForces.size(); i++){
            applyImpulse(temperatureBuffer, temporalForces[i].pos, ofVec3f(temporalForces[i].temp,temporalForces[i].temp,temporalForces[i].temp), temporalForces[i].rad);
            if (temporalForces[i].color.length() != 0)
                applyImpulse(pingPong, temporalForces[i].pos, temporalForces[i].color * temporalForces[i].den, temporalForces[i].rad);
            if (temporalForces[i].vel.length() != 0)
                applyImpulse(velocityBuffer , temporalForces[i].pos, temporalForces[i].vel, temporalForces[i].rad);
        }
        temporalForces.clear();
    }
    
    if ( constantForces.size() != 0){
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofDisableBlendMode();
        for(int i = 0; i < constantForces.size(); i++){
            applyImpulse(temperatureBuffer, constantForces[i].pos, ofVec3f(constantForces[i].temp,constantForces[i].temp,constantForces[i].temp), constantForces[i].rad);
            if (constantForces[i].color.length() != 0)
                applyImpulse(pingPong, constantForces[i].pos, constantForces[i].color * constantForces[i].den, constantForces[i].rad);
            if (constantForces[i].vel.length() != 0)
                applyImpulse(velocityBuffer , constantForces[i].pos, constantForces[i].vel, constantForces[i].rad);
        }
    }
    
    //  Compute
    //
    computeDivergence();
    pressureBuffer.src->begin();
    ofClear(0);
    pressureBuffer.src->end();
    
    for (int i = 0; i < numJacobiIterations; i++) {
        jacobi();
        pressureBuffer.swap();
    }
    
    subtractGradient();
    velocityBuffer.swap();
}

void ofxFluid::draw(int x, int y, float _width, float _height){
    if (_width == -1) _width = width;
    if (_height == -1) _height = height;
    
    ofPushStyle();
    ofSetColor(255);
    
//    ofEnableAlphaBlending();
//    if(bObstacles){
//        textures[0].draw(x,y,_width,_height);
//    }
    
    pingPong.src->draw(x,y,_width,_height);

    ofPopStyle();
}

void ofxFluid::drawVelocity(int x, int y, float _width, float _height){
    if (_width == -1) _width = width;
    if (_height == -1) _height = height;
    
    ofPushStyle();
    ofSetColor(255);
    velocityBuffer.src->draw(x,y,_width,_height);
    ofPopStyle();
}


void ofxFluid::advect(ofxSwapBuffer& _buffer, float _dissipation){
    _buffer.dst->begin();
    shader.begin();
    shader.setUniform1f("TimeStep", timeStep);
    shader.setUniform1f("Dissipation", _dissipation);
    shader.setUniformTexture("VelocityTexture", velocityBuffer.src->getTextureReference(), 0);
    shader.setUniformTexture("backbuffer", _buffer.src->getTextureReference(), 1);
    shader.setUniformTexture("tex0", obstaclesFbo.getTextureReference(), 2);
    renderFrame(gridWidth,gridHeight);
    shader.end();
    _buffer.dst->end();
}

void ofxFluid::jacobi(){
    pressureBuffer.dst->begin();
    jacobiShader.begin();
    jacobiShader.setUniform1f("Alpha", -cellSize * cellSize);
    jacobiShader.setUniform1f("InverseBeta", 0.25f);
    jacobiShader.setUniformTexture("Pressure", pressureBuffer.src->getTextureReference(), 0);
    jacobiShader.setUniformTexture("Divergence", divergenceFbo.getTextureReference(), 1);
    jacobiShader.setUniformTexture("tex0", obstaclesFbo.getTextureReference(), 2);
    
    renderFrame(gridWidth,gridHeight);
    
    jacobiShader.end();
    pressureBuffer.dst->end();
}

void ofxFluid::subtractGradient(){
    velocityBuffer.dst->begin();
    subtractGradientShader.begin();
    subtractGradientShader.setUniform1f("GradientScale", gradientScale);
    
    subtractGradientShader.setUniformTexture("Velocity", velocityBuffer.src->getTextureReference(), 0);
    subtractGradientShader.setUniformTexture("Pressure", pressureBuffer.src->getTextureReference(), 1);
    subtractGradientShader.setUniformTexture("tex0", obstaclesFbo.getTextureReference(), 2);
    
    renderFrame(gridWidth,gridHeight);
    
    subtractGradientShader.end();
    velocityBuffer.dst->end();
}

void ofxFluid::computeDivergence(){
    divergenceFbo.begin();
    computeDivergenceShader.begin();
    computeDivergenceShader.setUniform1f("HalfInverseCellSize", 0.5f / cellSize);
    computeDivergenceShader.setUniformTexture("Velocity", velocityBuffer.src->getTextureReference(), 0);
    computeDivergenceShader.setUniformTexture("tex0", obstaclesFbo.getTextureReference(), 1);
    
    renderFrame(gridWidth,gridHeight);

    computeDivergenceShader.end();
    divergenceFbo.end();
}

void ofxFluid::applyImpulse(ofxSwapBuffer& _buffer, ofBaseHasTexture &_baseTex, float _pct, bool _isVel){
    glEnable(GL_BLEND);
    _buffer.dst->begin();
    applyTextureShader.begin();
    applyTextureShader.setUniformTexture("backbuffer", *(_buffer.src), 0);
    applyTextureShader.setUniformTexture("tex1", _baseTex.getTexture(), 1);
    applyTextureShader.setUniform1f("pct", _pct);
    applyTextureShader.setUniform1i("isVel", (_isVel)?1:0);
    renderFrame(gridWidth,gridHeight);
    applyTextureShader.end();
    _buffer.dst->end();
    _buffer.swap();
    glDisable(GL_BLEND);
}

void ofxFluid::applyImpulse(ofxSwapBuffer& _buffer, ofPoint _force, ofPoint _value, float _radio){
    glEnable(GL_BLEND);
    _buffer.src->begin();
    applyImpulseShader.begin();
    
    applyImpulseShader.setUniform2f("Point", (float)_force.x, (float)_force.y);
    applyImpulseShader.setUniform1f("Radius", (float) _radio );
    applyImpulseShader.setUniform3f("Value", (float)_value.x, (float)_value.y, (float)_value.z);
    
    renderFrame(gridWidth,gridHeight);
    
    applyImpulseShader.end();
    _buffer.src->end();
    glDisable(GL_BLEND);
}

void ofxFluid::applyBuoyancy(){
    velocityBuffer.dst->begin();
    applyBuoyancyShader.begin();
    applyBuoyancyShader.setUniform1f("AmbientTemperature", ambientTemperature );
    applyBuoyancyShader.setUniform1f("TimeStep", timeStep );
    applyBuoyancyShader.setUniform1f("Sigma", smokeBuoyancy );
    applyBuoyancyShader.setUniform1f("Kappa", smokeWeight );
    
    applyBuoyancyShader.setUniform2f("Gravity", (float)gForce.x, (float)gForce.y );
    
    applyBuoyancyShader.setUniformTexture("Velocity", velocityBuffer.src->getTextureReference(), 0);
    applyBuoyancyShader.setUniformTexture("Temperature", temperatureBuffer.src->getTextureReference(), 1);
    applyBuoyancyShader.setUniformTexture("Density", pingPong.src->getTextureReference(), 2);
    
    renderFrame(gridWidth,gridHeight);
    
    applyBuoyancyShader.end();
    velocityBuffer.dst->end();
}