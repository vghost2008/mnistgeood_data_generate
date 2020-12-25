#include <QtCore>
#include <QtGui>
#include <stdlib.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <string>
#include <set>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>
#include <QApplication>
#include <time.h>
#include "boost/property_tree/json_parser.hpp"

using namespace std;
namespace pt = boost::property_tree;
template<typename T, int N>
inline size_t array_size(const T (&)[N]) noexcept { return N; }


int begin_num    = 0;
int max_instance_num = 5;
int img_nr       = 100;
string save_dir = "./data";
constexpr auto kImgWidth = 300;
constexpr auto kImgHeight = 300;

enum ADType
{
	ADT_NSNN=1, //1
	ADT_SNN, //2
	ADT_SUN, //3
	ADT_SNU, //4
	ADT_SUU, //5
	ADT_BNN, //6
	ADT_NBNN, //7
	ADT_BUN, //8
	ADT_BNU, //9
	ADT_BUU, //10
	ADT_NR,
};

using AnnotationData=std::tuple<QRect,ADType,pair<vector<QPoint>,vector<QPoint>>>;
int random_in_range(int min,int max)
{
	if(min>=max) return min;
	return (rand()%(max-min))+min;
}
std::vector<QRect> getRandomBox(const QRect& rect,int w_min,int w_max,int h_min, int h_max) 
{
	auto       w = random_in_range(w_min,w_max);
	auto       h = random_in_range(h_min,h_max);

	if(h>w*1.5)
		h=w*1.5;
	else if(w>h*1.5)
		w = h*1.5;

	const auto x = random_in_range(0,rect.width()-w);
	const auto y = random_in_range(0,rect.height()-h);
	return vector<QRect>(1,QRect(x+rect.x(),y+rect.y(),w,h));
}
QImage createPicture(int w=300,int h=300)
{
	QImage pic(w,h,QImage::Format_RGB888);
	QPainter painter(&pic);
	painter.fillRect(0,0,w,h,Qt::black);
	return pic;
}
QImage getImage(const QImage& src, const QRect& rect)
{
	QImage     res = createPicture(rect.width(),rect.height());
	QPainter   painter(&res);

	painter.drawImage(QRect(0,0,rect.width(),rect.height()),src,rect);
	return res;
}
bool isAllZero(const QImage& pic,int row,const QRect& rect)
{
	const auto end = rect.x()+rect.width();
	for(int i=rect.x(); i<end; ++i) {
		if(pic.pixelColor(i,row) != Qt::black) return false;
	}
	return true;
}
int topUnZero(const QImage& pic,const QRect& rect)
{
	auto i = rect.y()+rect.height()/3;
	for(; i>rect.y(); --i)
		if(isAllZero(pic,i,rect)) return i;
	return i;
}
int bottomUnZero(const QImage& pic,const QRect& rect)
{
	auto i = rect.y()+rect.height()*2/3;
	const auto end = rect.y()+rect.height();
	for(; i<end; ++i)
		if(isAllZero(pic,i,rect)) return i;
	return i;
}
typedef vector<QRect> (*GetRandomBoxsFunc)(const QRect& rect,int,int,int,int);
/*
 * R:row num
 * C:column num
 */
template<int R,int C>
vector<QRect> getGRandomBoxs(const QRect& rect,int w_min, int w_max, int h_min, int h_max)
{
	w_min = std::min<int>(w_min,kImgWidth/C-10);
	h_min = std::min<int>(h_min,kImgHeight/R-10);
	w_max = std::min<int>(w_max,kImgWidth/C-5);
	h_max = std::min<int>(h_max,kImgHeight/R-5);
	auto w = rect.width()/C;
	auto h = rect.height()/R;
	vector<QRect> res;

	for(int i=0; i<C; ++i) {
		auto x_offset = i*w;
		for(int j=0; j<R; ++j) {
			auto y_offset = j*h;
			res.push_back(getRandomBox(QRect(x_offset,y_offset,w,h),w_min,w_max,h_min,h_max).front());
		}
	}
	return res;
}
vector<QRect> getMultiRandomBoxs(const QRect& rect,int w_min, int w_max, int h_min, int h_max,int size)
{
	GetRandomBoxsFunc funcs[] = {getRandomBox,
		getGRandomBoxs<1,2>, getGRandomBoxs<2,1>,
		getGRandomBoxs<3,1>,getGRandomBoxs<1,3>,
		getGRandomBoxs<2,2>,getGRandomBoxs<4,1>,getGRandomBoxs<1,4>,
		getGRandomBoxs<2,3>,getGRandomBoxs<3,2>,
		getGRandomBoxs<2,4>,getGRandomBoxs<4,2>,
		getGRandomBoxs<3,3>,
		getGRandomBoxs<2,5>, getGRandomBoxs<5,2>,
		getGRandomBoxs<2,6>, getGRandomBoxs<6,2>, getGRandomBoxs<3,4>, getGRandomBoxs<4,3>,
		};
	const size_t sizes[] = {1,2,2,3,3,4,4,4,6,6,8,8,9,10,10,12,12,12,12};
	const auto s = distance(begin(sizes),upper_bound(begin(sizes),end(sizes),size));
	const auto index = random_in_range(0,s);
	return funcs[index](rect,w_min,w_max,h_min,h_max);
}
vector<QPoint> getEllipsePoints(const QRect& rect,bool normal=false)
{
    vector<QPoint> points;
    const auto center = rect.center();
    const auto b = rect.width()/2;
    const auto a = rect.height()/2;
    const auto delta = max<int>(max(a,b)*0.25,6);
    const auto kNr = 120;
    auto da = delta*0.5;
    auto db = delta*0.5;

    for(auto i=0; i<kNr; ++i) {
        auto ta = a;
        auto tb = b;
        if(!normal) {
            ta +=  da;
            tb +=  db;
        }
        const auto _x = tb*cos(M_PI*2*i/kNr);
        const auto _y = ta*sin(M_PI*2*i/kNr);

        points.push_back(QPoint(_x,_y)+center);
        if(!normal) {
            da = da*0.5+ (rand()%delta-delta/2)*0.5;
            db = db*0.5+ (rand()%delta-delta/2)*0.5;
        }
    }
    return points;
}
pair<vector<QPoint>,vector<QPoint>> getDoubleEllipsePoints(const QRect& rect,bool small_kernel=true,bool normal_kernel=false,bool normal=false)
{
    auto points0 = getEllipsePoints(rect,normal);
    auto center = rect.center();
    auto w = 0;
    auto h = 0;

    if(small_kernel) {
        w = random_in_range(rect.width()*0.3,rect.width()*0.5);
        h = random_in_range(rect.height()*0.3,rect.height()*0.5);
    } else {
        w = random_in_range(rect.width()*0.7,rect.width()*0.92);
        h = random_in_range(rect.height()*0.7,rect.height()*0.92);
    }

    QRect rect0(center.x()-w/2,center.y()-h/2,w,h);
    auto points1 = getEllipsePoints(rect0,normal_kernel);

    return make_pair(points0,points1);
}
QRect getCircleRect(const QRect& rect)
{
    auto size = min(rect.width(),rect.height());
    return QRect(rect.left(),rect.top(),size,size);
}
QRect getEllipseRect(const QRect& rect)
{
    auto min_size = min(rect.width(),rect.height());
    auto max_size = max(rect.width(),rect.height());
    if(min_size>(max_size/1.1)) {
        min_size = max_size/1.1;
        if(rect.width()>rect.height()) {
            return QRect(rect.left(),rect.top(),max_size,min_size);
        } else {
            return QRect(rect.left(),rect.top(),min_size,max_size);
        }
    } else {
        return rect;
    }
}
AnnotationData drawObjInPicture(QImage& pic, QRect rect)
{
    const auto type = (rand()%(ADT_NR-1))+1;
    QPainter   painter(&pic);
    pair<vector<QPoint>,vector<QPoint>>  points;
    switch(type) {
        case ADT_NSNN:
        case ADT_NBNN:
            rect = getCircleRect(rect);
        break;
        default:
            rect = getEllipseRect(rect);
        break;
    }
    switch(type) {
        case ADT_NSNN:
            points = getDoubleEllipsePoints(rect,true,true,true);
            break;
        case ADT_SNN:
            points = getDoubleEllipsePoints(rect,true,true,true);
            break;
        case ADT_SUN:
            points = getDoubleEllipsePoints(rect,true,false,true);
            break;
        case ADT_SNU:
            points = getDoubleEllipsePoints(rect,true,true,false);
            break;
        case ADT_SUU:
            points = getDoubleEllipsePoints(rect,true,false,false);
            break;
        case ADT_NBNN:
            points = getDoubleEllipsePoints(rect,false,true,true);
            break;
        case ADT_BNN:
            points = getDoubleEllipsePoints(rect,false,true,true);
            break;
        case ADT_BUN:
            points = getDoubleEllipsePoints(rect,false,false,true);
            break;
        case ADT_BNU:
            points = getDoubleEllipsePoints(rect,false,true,false);
            break;
        case ADT_BUU:
            points = getDoubleEllipsePoints(rect,false,false,false);
            break;
    }

    QColor color0(rand()%200+25,rand()%200+25,rand()%200+25);

    painter.setPen(color0);
    painter.setBrush(color0);
    painter.drawPolygon(points.first.data(),points.first.size());
    QColor color1(255-color0.red(),rand()%200+25,rand()%200+25);
    painter.setPen(color1);
    painter.setBrush(color1);
    painter.drawPolygon(points.second.data(),points.second.size());

    return AnnotationData(rect,ADType(type),points);
}
std::vector<AnnotationData> drawMultiObjInPicture(QImage& pic, int w_min, int w_max, int h_min, int h_max,int size)
{
	auto                   rects     = getMultiRandomBoxs(QRect(QPoint(0,0),pic.size()),w_min,w_max,h_min,h_max,size);
	vector<AnnotationData> res;

	for(auto i=0; i<rects.size(); ++i) {
		res.push_back(drawObjInPicture(pic,rects[i]));
	}

	return res;
}
void writeAnnotation(const QImage& pic,std::vector<AnnotationData>& rects,const string& file_path)
{
	auto rect = pic.size();
	pt::ptree tree;
	tree.put("annotation.filename",QFileInfo(file_path.c_str()).baseName().toUtf8().data());
	tree.put("annotation.size.width",rect.width());
	tree.put("annotation.size.height",rect.height());
	tree.put("annotation.size.depth",3);
	for(auto& r:rects) {
		pt::ptree ctree;
		const auto& re= std::get<0>(r);
		ctree.put("name",int(std::get<1>(r)));

		ctree.put("bndbox.xmin",re.x());
		ctree.put("bndbox.ymin",re.y());
		ctree.put("bndbox.xmax",re.x()+re.width());
		ctree.put("bndbox.ymax",re.y()+re.height());
		tree.add_child("annotation.object",ctree);
	}
	boost::property_tree::xml_parser::xml_writer_settings<string> settings(' ',2);
	pt::write_xml(file_path,tree,std::locale(),settings);
}
void writeJsonAnnotation(const QImage& pic,std::vector<AnnotationData>& datas,const string& file_path)
{
	try {
		auto rect = pic.size();
		boost::property_tree::ptree pt_root;
		pt_root.add("version", "3.10.1");

		boost::property_tree::ptree pt_shapes;
		for (auto& d:datas) {
			boost::property_tree::ptree pt_shape;
			boost::property_tree::ptree pt_points;
			const auto type = std::get<1>(d);
			pt_shape.add("label", type);
			pt_shape.add("shape_type", "polygon");
			const auto points = std::get<2>(d).first;
			boost::property_tree::ptree pt_array0;
			for(auto& p:points) {
				boost::property_tree::ptree pt_array1;
				boost::property_tree::ptree pt_array1_1;
				boost::property_tree::ptree pt_array2;
				boost::property_tree::ptree pt_array3;
				pt_array2.put("",p.x());
				pt_array3.put("",p.y());
				pt_array1_1.push_back(std::make_pair("",pt_array2));
				pt_array1_1.push_back(std::make_pair("",pt_array3));
				pt_array1.push_back(std::make_pair("",pt_array1_1));
				pt_array0.push_back(std::make_pair("",pt_array1));
			}
			pt_shape.add_child("points",pt_array0);
			pt_shapes.push_back(std::make_pair("",pt_shape));
		}
		pt_root.add_child("shapes",pt_shapes);
		pt_root.add("imagePath",QFileInfo(file_path.c_str()).baseName().toUtf8().data());
		pt_root.add("imageWidth",rect.width());
		pt_root.put("imageHeight",rect.height());
		boost::property_tree::json_parser::write_json(file_path.c_str(), pt_root);
	}
    catch (std::exception& e) {
		cout<<"Error"<<endl;
    }
}
int main(int argc,char** argv)
{
	QApplication app(argc,argv);
	srand(time(nullptr));
	for(int i=1; i<argc; ++i) {
		if(0 == strcmp(argv[i],"-bn")) { //开始序号
			begin_num = stol(argv[++i]);
		} else if(0 == strcmp(argv[i],"-max_instance_nr")) { //图像数量
			max_instance_num = stol(argv[++i]);
		} else if(0 == strcmp(argv[i],"-nr")) { //图像大小
			img_nr = stol(argv[++i]);
		} else if(0 == strcmp(argv[i],"-save_dir")) { //保存目录
			save_dir = argv[++i];
		} else {
            cout<<"Error arg:"<<argv[i]<<endl;
        }
	}
    cout<<"save dir:"<<save_dir<<endl;
    cout<<"data nr:"<<img_nr<<endl;

	const auto image_dir = save_dir.c_str();
	const auto annotations_dir = save_dir.c_str();

	if(!QFile::exists(image_dir)) 
		QDir().mkdir(image_dir);
	if(!QFile::exists(annotations_dir)) 
		QDir().mkdir(annotations_dir);

	auto       end_num = begin_num+img_nr;
	const auto w_min   = 48;
	const auto w_max   = 224;
	const auto h_min   = 48;
	const auto h_max   = 224;
#if 0
	const auto w_min   = 224;
	const auto w_max   = 298;
	const auto h_min   = 224;
	const auto h_max   = 298;
#endif
	cout<<endl;
	for(auto i=begin_num; i<end_num; ++i) {
		stringstream ss;
		ss<<image_dir<<"/IMG_"<<setfill('0')<<setw(6)<<i<<".jpg";
		stringstream ss_a;
		ss_a<<annotations_dir<<"/IMG_"<<setfill('0')<<setw(6)<<i<<".xml";
		stringstream ss_b;
		ss_b<<annotations_dir<<"/IMG_"<<setfill('0')<<setw(6)<<i<<".json";
		auto pic = createPicture();
		auto anno_data = drawMultiObjInPicture(pic,w_min,w_max,h_min,h_max,max_instance_num);
		writeAnnotation(pic,anno_data,ss_a.str());
		writeJsonAnnotation(pic,anno_data,ss_b.str());
		pic.save(ss.str().c_str(),"jpg");
		cout<<"\r"<<i;
	}
	cout<<endl;
}
