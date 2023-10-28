#coding: utf-8
import sys
import cv2
import numpy as np

filename=sys.argv[1]

img = cv2.imread(filename, cv2.IMREAD_COLOR)

size = ( 270, 150 )

halfImg = cv2.resize( img, size )

halfImg = cv2.rotate( halfImg, cv2.ROTATE_90_COUNTERCLOCKWISE )
halfImg = cv2.rotate( halfImg, cv2.ROTATE_90_COUNTERCLOCKWISE )
halfImg = cv2.rotate( halfImg, cv2.ROTATE_90_COUNTERCLOCKWISE )


cv2.imwrite('resizeImg2.jpg', halfImg)

print("static const uint32_t LCD2_PIXEL_DATA[ 150 * 270 ] =\n")
print("{\n")
for v in range( halfImg.shape[ 0 ] ):
    for h in range ( halfImg.shape[ 1 ] ):
        tmp = ( ( ( halfImg[ v ][ h ][ 2 ] * ( 2**5 - 1 ) ) // 255 )* ( 2**11 ) ) + ( ( ( halfImg[ v ][ h ][ 1 ] * ( 2**6 - 1 ) ) // 255 ) * ( 2**5 ) ) + ( ( halfImg[ v ][ h ][ 0 ] * ( 2**5 - 1) ) // 255 )
        print( '/*', v, ',', h, '*/',  tmp, ',' )
print("};\n")

