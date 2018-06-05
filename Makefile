TARGET = e.update
SOURCE = Sample_update.cpp libcyusb.cpp download_fx3.cpp
all:
	g++ $(SOURCE) -o $(TARGET) -lusb-1.0 -L. -lMoveSenseCamera -ludev  
Debug:
	g++ $(SOURCE) -o $(TARGET) -lusb-1.0 -L. -lMoveSenseCamera -ludev -g 
clean:
	rm $(TARGET)
