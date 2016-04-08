all: cctree

cctree: simulator.cpp cctree.cpp cctree.h heap.cpp heap.h
	g++ -O9 -o cctree simulator.cpp cctree.cpp heap.cpp

deathTracker: deathTracker.cpp  cctree.cpp cctree.h heap.cpp heap.h EdgeBundler.cpp EdgeBundler.h
	g++ -O9 -o deathTracker deathTracker.cpp cctree.cpp heap.cpp EdgeBundler.cpp 

unioner: unioner.cpp  cctree.cpp cctree.h heap.cpp heap.h EdgeBundler.cpp EdgeBundler.h
	g++ -O9 -o unioner unioner.cpp cctree.cpp heap.cpp EdgeBundler.cpp 

specunioner: specunioner.cpp  cctree.cpp cctree.h heap.cpp heap.h EdgeBundler.cpp EdgeBundler.h
	g++ -O9 -o specunioner specunioner.cpp cctree.cpp heap.cpp EdgeBundler.cpp 

keyObjectFinder: keyObjectFinder.cpp  heap.cpp heap.h 
	g++ -g -o keyObjectFinder keyObjectFinder.cpp heap.cpp EdgeBundler.cpp -L../HashGraph -I../HashGraph -lHashGraph 

trace2db: trace2db.cpp  cctree.cpp cctree.h heap.cpp heap.h
	g++ -O9 -o trace2db trace2db.cpp cctree.cpp heap.cpp -lpq -I/usr/include/postgresql

whoDiedAtTheSameTime: whoDiedAtTheSameTime.cpp  cctree.cpp cctree.h heap.cpp heap.h
	g++ -O9 -o whoDiedAtTheSameTime whoDiedAtTheSameTime.cpp cctree.cpp heap.cpp 

oad: oad.cpp 
	g++ -O9 -o oad oad.cpp 

identity: identity_callback_handler.cpp identity.cpp parser.cpp
	g++ -O9 -o identity identity_callback_handler.cpp identity.cpp parser.cpp

lifetime_correlation: lifetime_correlation_callback_handler.h lifetime_correlation_callback_handler.cpp parser.cpp lifetime_correlation.cpp
	g++ -O9 -o lifetime_correlation lifetime_correlation_callback_handler.cpp parser.cpp lifetime_correlation.cpp


lifetime_correlation_ismm2013: lifetime_correlation_callback_handler.h lifetime_correlation_callback_handler.cpp parser.cpp lifetime_correlation.cpp
	g++ -O9 -o lifetime_correlation_ismm2013 lifetime_correlation_callback_handler.cpp ./ismm2013/parser_ismm2013.cpp lifetime_correlation.cpp

escape_analysis: escape_analysis_callback_handler.h escape_analysis_callback_handler.cpp parser.cpp escape_analysis.cpp
	g++ -O9 -o escape_analysis escape_analysis_callback_handler.cpp parser.cpp escape_analysis.cpp


escape_analysis_ismm2013: escape_analysis_callback_handler.h escape_analysis_callback_handler.cpp ./ismm2013/parser_ismm2013.h ./ismm2013/parser_ismm2013.cpp escape_analysis.cpp
	g++ -O9 -o escape_analysis_ismm2013 escape_analysis_callback_handler.cpp ./ismm2013/parser_ismm2013.cpp escape_analysis.cpp

clean:
	rm -f *.o
