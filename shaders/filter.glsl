// Adapted from Vision Pro's filterCovariance. Shared normalization prevents
// large near-camera ellipses from overflowing the determinant products.
float filterOpacity(inout float a,inout float b,inout float d,float variance,bool compensated){
    if(isnan(a)||isinf(a)||isnan(b)||isinf(b)||isnan(d)||isinf(d)||a<0||d<0)return 0.0;
    if(!compensated){a+=variance;d+=variance;return 1.0;}
    float magnitude=max(max(a,d),max(abs(b),variance));
    float na=a/magnitude,nb=b/magnitude,nd=d/magnitude,padding=variance/magnitude;
    float determinant=max(0.0,na*nd-nb*nb);
    float filtered=(na+padding)*(nd+padding)-nb*nb;
    a+=variance;d+=variance;
    return filtered>0?sqrt(clamp(determinant/filtered,0.0,1.0)):0.0;
}
