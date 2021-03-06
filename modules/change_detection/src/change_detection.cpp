/*
 * change_detection.cpp
 *
 *  Created on: 28.1.2016
 *      Author: ivelas
 */

#include <cstdlib>
#include <iostream>

#include <glog/logging.h>
#include <pcl/point_cloud.h>
#include <pcl/common/eigen.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/registration/distances.h>

#include <v4r/change_detection/occlusion_checker.h>
#include <v4r/change_detection/viewport_checker.h>
#include <v4r/change_detection/change_detection.h>
#include <v4r/change_detection/visualizer.h>

namespace v4r
{

template<class PointT>
void ChangeDetector<PointT>::detect(const typename pcl::PointCloud<PointT>::ConstPtr &old_scene, const CloudPtr new_scene,
        const Eigen::Affine3f sensor_pose,
        float diff_tolerance)
{
    added->clear();
    removed->clear();

    // compute differences and check occlusions
    CloudPtr differenceNew(new pcl::PointCloud<PointT>());
    CloudPtr differenceOld(new pcl::PointCloud<PointT>());

    vector<int> indices_dummy;
    difference(*old_scene, new_scene, *differenceOld, indices_dummy, diff_tolerance);
    indices_dummy.clear();
    difference(*new_scene, old_scene, *differenceNew, indices_dummy, diff_tolerance);

    /*
    CloudPtr vis_raw_changes(new Cloud);
    CloudPtr vis_occ_test(new Cloud);
    CloudPtr vis_view_test(new Cloud);
    *vis_raw_changes += *old_scene;
    *vis_raw_changes += *new_scene;
    VisualResultsStorage::copyCloudColored(*differenceOld, *vis_raw_changes, 255, 0, 0);
    VisualResultsStorage::copyCloudColored(*differenceNew, *vis_raw_changes, 0, 255, 0);
     */

    *added += *(differenceNew);

    if(!differenceOld->empty()) {
        OcclusionChecker <PointT> occlusionChecker;
        Eigen::Vector3f sensor_origin = sensor_pose.translation();
        occlusionChecker.setViewpoint(sensor_origin); // since it's already transformed in the metaroom frame of ref
        occlusionChecker.setNumberOfBins(params.occlusion_checker_bins);
        typename OcclusionChecker<PointT>::occlusion_results occlusions_old = occlusionChecker.checkOcclusions(
                differenceOld, new_scene);

        /*
        *vis_occ_test += *old_scene;
        *vis_occ_test += *new_scene;
        v4r::VisualResultsStorage::copyCloudColored(*occlusions_old.nonOccluded, *vis_occ_test, 255, 0, 0);
        v4r::VisualResultsStorage::copyCloudColored(*added, *vis_occ_test, 0, 255, 0);
        pcl::io::savePCDFile("after-occlusion-test.pcd", *vis_occ_test, true);
         */

        ViewportChecker<PointT> viewport_check;
        ViewVolume<PointT> volume = ViewVolume<PointT>::ofXtion(sensor_pose);
        viewport_check.add(volume);
        CloudPtr outside(new pcl::PointCloud<PointT>());
        viewport_check.getVisibles(occlusions_old.nonOccluded, removed, outside);

        /*
        *vis_view_test += *old_scene;
        *vis_view_test += *new_scene;
        v4r::VisualResultsStorage::copyCloudColored(*removed, *vis_view_test, 255, 0, 0);
        v4r::VisualResultsStorage::copyCloudColored(*added, *vis_view_test, 0, 255, 0);
        pcl::io::savePCDFile("after-view-vol-test.pcd", *vis_view_test, true);
         */
    }

    /*
    Visualizer3D vis;
    int viewport_raw = 0;
    int viewport_occlusion = 1;
    int viewport_view_check = 2;
    vis.getViewer()->createViewPort(0.0, 0.0, 0.33, 1.0, viewport_raw);
    vis.getViewer()->setBackgroundColor(255, 255, 255, viewport_raw);
    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> handler_raw(vis_raw_changes);
    vis.getViewer()->addPointCloud<pcl::PointXYZRGB> (vis_raw_changes, handler_raw, "raw_diff", viewport_raw);
    vis.getViewer()->createViewPort(0.33, 0.0, 0.67, 1.0, viewport_occlusion);
    vis.getViewer()->setBackgroundColor(255, 255, 255, viewport_occlusion);
    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> handler_occ(vis_occ_test);
    vis.getViewer()->addPointCloud<pcl::PointXYZRGB> (vis_occ_test, handler_occ, "occlusion_test", viewport_occlusion);
    vis.getViewer()->createViewPort(0.67, 0.0, 1.0, 1.0, viewport_view_check);
    vis.getViewer()->setBackgroundColor(255, 255, 255, viewport_view_check);
    pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> handler_view(vis_view_test);
    vis.getViewer()->addPointCloud<pcl::PointXYZRGB> (vis_view_test, handler_view, "view_vol_test", viewport_view_check);
    ViewVolume<PointT> vis_volume = ViewVolume<PointT>::ofXtion(sensor_pose, 0.0);
    pcl::PointCloud<pcl::PointXYZ> vol_borders = vis_volume.getBorders();
    for(int i = 0; i < 4; i++) {
        stringstream ss1; ss1 << "near_" << i;
        vis.getViewer()->addLine(vol_borders[i], vol_borders[(i+1)%4], .6, .0, .6, ss1.str(), viewport_view_check);
        stringstream ss2; ss2 << "far_" << i;
        vis.getViewer()->addLine(vol_borders[i+4], vol_borders[(i+1)%4+4], .6, .0, .6, ss2.str(), viewport_view_check);
        stringstream ss3; ss3 << "cross_" << i;
        vis.getViewer()->addLine(vol_borders[i], vol_borders[i+4], .6, .0, .6, ss3.str(), viewport_view_check);
    }
    vis.show();
     */
}

template<class PointT>
bool ChangeDetector<PointT>::isObjectRemoved(CloudPtr object_cloud) const
{
    if(removed->empty())
        return false;

    CloudPtr support = removalSupport(object_cloud);

    // TODO better reasoning
    return (support->size() > 50);
}

template<class PointT>
typename ChangeDetector<PointT>::CloudPtr ChangeDetector<PointT>::removalSupport(
        CloudPtr &object_cloud) const {
    return removalSupport(removed, object_cloud);
}

template<class PointT>
int ChangeDetector<PointT>::overlapingPoints(const CloudPtr train, const CloudPtr query,
        float tolerance)
{
    CloudPtr queryMinusTrain(new pcl::PointCloud<PointT>());
    vector<int> indices;
    difference(*query, train, *queryMinusTrain, indices, tolerance);
    return query->size() - queryMinusTrain->size();
}

template<class PointT>
void ChangeDetector<PointT>::removePointsFrom(const CloudPtr cloud, const CloudPtr toBeRemoved)
{
    CloudPtr original(new pcl::PointCloud<PointT>());
    *original += *cloud;
    cloud->clear();
    difference(*original, toBeRemoved, *cloud);
}


/**
 * from interval [0.0, 1.0]
 */
template<class PointT>
float
ChangeDetector<PointT>::computePlanarity(const typename pcl::PointCloud<PointT>::ConstPtr input_cloud)
{
    pcl::SACSegmentation <PointT> seg;
    pcl::PointIndices inliers;
    pcl::ModelCoefficients coefficients;

    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setMaxIterations(10);
    seg.setDistanceThreshold(DEFAULT_PARAMETERS.cloud_difference_tolerance);

    seg.setInputCloud(input_cloud);
    seg.segment(inliers, coefficients);
    return float(inliers.indices.size()) / input_cloud->size();
}

template<class PointT>
std::vector<typename pcl::PointCloud<PointT>::Ptr>
ChangeDetector<PointT>::clusterPointCloud(
        CloudPtr input_cloud, double tolerance, int min_cluster_size, int max_cluster_size)
{
    typename Tree::Ptr tree(new pcl::search::KdTree<PointT>);
    tree->setInputCloud(input_cloud);
    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<PointT> ec;
    ec.setClusterTolerance(tolerance);
    ec.setMinClusterSize(min_cluster_size);
    ec.setMaxClusterSize(max_cluster_size);
    ec.setSearchMethod(tree);
    ec.setInputCloud(input_cloud);
    ec.extract(cluster_indices);

    std::vector<CloudPtr> toRet;

    int j = 0;
    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
    {
        CloudPtr cloud_cluster(new pcl::PointCloud<PointT>());
        for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); pit++)
            cloud_cluster->points.push_back(input_cloud->points[*pit]); //*
        cloud_cluster->width = cloud_cluster->points.size();
        cloud_cluster->height = 1;
        cloud_cluster->is_dense = true;

        toRet.push_back(cloud_cluster);

        j++;
    }

    return toRet;
}

template<class PointType>
const ChangeDetectorParameters ChangeDetector<PointType>::DEFAULT_PARAMETERS;

}

template class V4R_EXPORTS v4r::ChangeDetector<pcl::PointXYZRGB>;
