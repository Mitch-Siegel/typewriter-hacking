# Imports PIL module
from PIL import Image

# open method used to open different extension image file
im = Image.open(input("Path of image to open:"))

needNewRun = True;
currentCommand = 0
currentRL = 0

totalPixelsDark = 0

for y in range(im.size[1]):
    for x in range(im.size[0]):
        value = im.getpixel((x, y)) == 0

        if(needNewRun):
            needNewRun = False
            # anything darker than all white will be black
            if value:
                currentCommand = 1
                currentRL = 1
                print("1", end='')
            else:
                currentCommand = 0
                currentRL = 1
                print("0", end='')

        elif (currentRL == 9) or (value != currentCommand):
            if(currentCommand == 1):
                totalPixelsDark += currentRL
            print(currentRL, end='')
            if value:
                currentCommand = 1
                currentRL = 1
                print("1", end='')
            else:
                currentCommand = 0
                currentRL = 1
                print("0", end='')
        else:
            currentRL += 1

        
        

    if(value == 1):
        totalPixelsDark += currentRL 
    print(currentRL, end='')
    needNewRun = True
    print("n ", end='');

print("{} pixels to draw".format(totalPixelsDark))