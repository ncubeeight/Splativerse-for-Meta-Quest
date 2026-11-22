#version 450
layout(location=0) in vec2 gaussian;
layout(location=1) in vec4 color;
layout(location=0) out vec4 outColor;
void main(){
    float alpha=min(0.99,color.a*exp(-0.5*dot(gaussian,gaussian)));
    if(isnan(alpha)||isinf(alpha)||alpha<1.0/255.0||any(isnan(color.rgb))||any(isinf(color.rgb)))discard;
    outColor=vec4(color.rgb*alpha,alpha);
}
