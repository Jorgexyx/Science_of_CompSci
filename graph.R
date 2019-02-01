data<-read.table("file.txt", header=TRUE)
x = data$Time
y = data$Sampling_Rate
jpeg("img.jpg")
plot(x,y, xlab="Wall-clock Peformance", ylab="Sampling Rate", main="Wall-Clock Peformance vs Sampling Rate")
dev.close()
