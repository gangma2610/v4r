﻿/*
 * hv_go_1.h
 *
 *  Created on: Feb 27, 2013
 *      Author: aitor
 */

#ifndef FAAT_PCL_GHV_H_
#define FAAT_PCL_GHV_H_

#include <pcl/common/common.h>
#include <pcl/pcl_macros.h>
#include "hypotheses_verification.h"
//#include <pcl/recognition/3rdparty/metslib/mets.hh>
#include <metslib/mets.hh>
#include <pcl/features/normal_3d.h>
#include <pcl/visualization/cloud_viewer.h>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <map>
#include <stack>
#include <iostream>
#include <fstream>
#include "ghv_opt.h"
#include <v4r/common/common_data_structures.h>

namespace v4r
{

  /** \brief A hypothesis verification method proposed in
   * "A Global Hypotheses Verification Method for 3D Object Recognition", A. Aldoma and F. Tombari and L. Di Stefano and Markus Vincze, ECCV 2012
   * \author Aitor Aldoma
   * Extended with physical constraints and color information (see ICRA paper)
   */

  template<typename ModelT, typename SceneT>
    class V4R_EXPORTS GHV : public HypothesisVerification<ModelT, SceneT>
    {
      friend class GHVmove_manager<ModelT, SceneT>;
      friend class GHVSAModel<ModelT, SceneT>;

      static float sRGB_LUT[256];
      static float sXYZ_LUT[4000];

      //////////////////////////////////////////////////////////////////////////////////////////////
      void
      RGB2CIELAB (unsigned char R, unsigned char G, unsigned char B, float &L, float &A,float &B2)
      {
        if (sRGB_LUT[0] < 0)
        {
          for (int i = 0; i < 256; i++)
          {
            float f = static_cast<float> (i) / 255.0f;
            if (f > 0.04045)
              sRGB_LUT[i] = powf ((f + 0.055f) / 1.055f, 2.4f);
            else
              sRGB_LUT[i] = f / 12.92f;
          }

          for (int i = 0; i < 4000; i++)
          {
            float f = static_cast<float> (i) / 4000.0f;
            if (f > 0.008856)
              sXYZ_LUT[i] = static_cast<float> (powf (f, 0.3333f));
            else
              sXYZ_LUT[i] = static_cast<float>((7.787 * f) + (16.0 / 116.0));
          }
        }

        float fr = sRGB_LUT[R];
        float fg = sRGB_LUT[G];
        float fb = sRGB_LUT[B];

        // Use white = D65
        const float x = fr * 0.412453f + fg * 0.357580f + fb * 0.180423f;
        const float y = fr * 0.212671f + fg * 0.715160f + fb * 0.072169f;
        const float z = fr * 0.019334f + fg * 0.119193f + fb * 0.950227f;

        float vx = x / 0.95047f;
        float vy = y;
        float vz = z / 1.08883f;

        vx = sXYZ_LUT[ std::min(int(vx*4000), 4000-1) ];
        vy = sXYZ_LUT[ std::min(int(vy*4000), 4000-1) ];
        vz = sXYZ_LUT[ std::min(int(vz*4000), 4000-1) ];

        L = 116.0f * vy - 16.0f;
        if (L > 100)
          L = 100.0f;

        A = 500.0f * (vx - vy);
        if (A > 120)
          A = 120.0f;
        else if (A <- 120)
          A = -120.0f;

        B2 = 200.0f * (vy - vz);
        if (B2 > 120)
          B2 = 120.0f;
        else if (B2<- 120)
          B2 = -120.0f;
      }
    public:
      class V4R_EXPORTS Parameter : public HypothesisVerification<ModelT, SceneT>::Parameter
      {
      public:
          using HypothesisVerification<ModelT, SceneT>::Parameter::inliers_threshold_;
          using HypothesisVerification<ModelT, SceneT>::Parameter::resolution_;
          using HypothesisVerification<ModelT, SceneT>::Parameter::occlusion_thres_;
          using HypothesisVerification<ModelT, SceneT>::Parameter::zbuffer_scene_resolution_;
          using HypothesisVerification<ModelT, SceneT>::Parameter::zbuffer_self_occlusion_resolution_;
          using HypothesisVerification<ModelT, SceneT>::Parameter::self_occlusions_reasoning_;

          float color_sigma_ab_;
          float color_sigma_l_;
          float regularizer_;
          float radius_neighborhood_clutter_;
          float radius_normals_;
          float duplicy_weight_test_;
          float duplicity_curvature_max_;
          bool ignore_color_even_if_exists_;
          int max_iterations_; //max iterations without improvement
          float clutter_regularizer_;
          bool detect_clutter_;
          float res_occupancy_grid_;
          float w_occupied_multiple_cm_;
          bool use_super_voxels_;
          bool use_replace_moves_;
          int opt_type_;
          float active_hyp_penalty_;
          int multiple_assignment_penalize_by_one_;
          float d_weight_for_bad_normals_;
          bool use_clutter_exp_;
          bool use_histogram_specification_;
          bool use_points_on_plane_side_;
          float best_color_weight_;
          bool initial_status_;

          //smooth segmentation parameters
          double eps_angle_threshold_;
          int min_points_;
          float curvature_threshold_;
          float cluster_tolerance_;

          bool use_normals_from_visible_;

          Parameter (
                  float color_sigma_ab = 0.25f, // 0.5f
                  float color_sigma_l = 0.5f,
                  float regularizer = 1.f, // 3
                  float radius_neighborhood_clutter = 0.03f,
                  float radius_normals = 0.01f, // 0.02f
                  float duplicy_weight_test = 1.f,
                  float duplicity_curvature_max = 0.03f,
                  bool ignore_color_even_if_exists = true,
                  int max_iterations = 5000,
                  float clutter_regularizer =  5.f,
                  bool detect_clutter = true,
                  float res_occupancy_grid = 0.005f,
                  float w_occupied_multiple_cm = 2.f, //0.f
                  bool use_super_voxels = false,
                  bool use_replace_moves = true,
                  int opt_type = 2, // 0
                  float active_hyp_penalty = 0.05f, // 0.f
                  int multiple_assignment_penalize_by_one = 2,
                  float d_weight_for_bad_normals = 0.1f,
                  bool use_clutter_exp = false,
                  bool use_histogram_specification = false, // true
                  bool use_points_on_plane_side = true,
                  float best_color_weight = 0.8f,
                  bool initial_status = false,
                  double eps_angle_threshold = 0.25, //0.1f
                  int min_points = 20, // 100
                  float curvature_threshold = 0.04f,
                  float cluster_tolerance = 0.015f, //0.01f;
                  bool use_normals_from_visible = false
                  )
              :
                HypothesisVerification<ModelT, SceneT>::Parameter(),
                color_sigma_ab_ (color_sigma_ab),
                color_sigma_l_ (color_sigma_l),
                regularizer_ (regularizer),
                radius_neighborhood_clutter_ (radius_neighborhood_clutter),
                radius_normals_ (radius_normals),
                duplicy_weight_test_ (duplicy_weight_test),
                duplicity_curvature_max_ (duplicity_curvature_max),
                ignore_color_even_if_exists_ (ignore_color_even_if_exists),
                max_iterations_ (max_iterations),
                clutter_regularizer_ (clutter_regularizer),
                detect_clutter_ (detect_clutter),
                res_occupancy_grid_ (res_occupancy_grid),
                w_occupied_multiple_cm_ (w_occupied_multiple_cm),
                use_super_voxels_ (use_super_voxels),
                use_replace_moves_ (use_replace_moves),
                opt_type_ (opt_type),
                active_hyp_penalty_ (active_hyp_penalty),
                multiple_assignment_penalize_by_one_ (multiple_assignment_penalize_by_one),
                d_weight_for_bad_normals_ (d_weight_for_bad_normals),
                use_clutter_exp_ (use_clutter_exp),
                use_histogram_specification_ (use_histogram_specification),
                use_points_on_plane_side_ (use_points_on_plane_side),
                best_color_weight_ (best_color_weight),
                initial_status_ (initial_status),
                eps_angle_threshold_ (eps_angle_threshold),
                min_points_ (min_points),
                curvature_threshold_ (curvature_threshold),
                cluster_tolerance_ (cluster_tolerance),
                use_normals_from_visible_ (use_normals_from_visible)
          {}
      }param_;

    protected:
      using HypothesisVerification<ModelT, SceneT>::mask_;
      using HypothesisVerification<ModelT, SceneT>::scene_cloud_downsampled_;
      using HypothesisVerification<ModelT, SceneT>::scene_downsampled_tree_;
      using HypothesisVerification<ModelT, SceneT>::visible_models_;
      using HypothesisVerification<ModelT, SceneT>::visible_normal_models_;
      using HypothesisVerification<ModelT, SceneT>::visible_indices_;
      using HypothesisVerification<ModelT, SceneT>::complete_models_;
      using HypothesisVerification<ModelT, SceneT>::normals_set_;
      using HypothesisVerification<ModelT, SceneT>::requires_normals_;
      using HypothesisVerification<ModelT, SceneT>::occlusion_cloud_;
      using HypothesisVerification<ModelT, SceneT>::scene_cloud_;
      using HypothesisVerification<ModelT, SceneT>::scene_sampled_indices_;

      template<typename PointT, typename NormalT>
        inline void
        extractEuclideanClustersSmooth (const typename pcl::PointCloud<PointT> &cloud, const typename pcl::PointCloud<NormalT> &normals, float tolerance,
                                        const typename pcl::search::Search<PointT>::Ptr &tree, std::vector<pcl::PointIndices> &clusters, double eps_angle,
                                        float curvature_threshold, unsigned int min_pts_per_cluster,
                                        unsigned int max_pts_per_cluster = (std::numeric_limits<int>::max) ())
        {

          if (tree->getInputCloud ()->points.size () != cloud.points.size ())
          {
            PCL_ERROR("[pcl::extractEuclideanClusters] Tree built for a different point cloud dataset\n");
            return;
          }
          if (cloud.points.size () != normals.points.size ())
          {
            PCL_ERROR("[pcl::extractEuclideanClusters] Number of points in the input point cloud different than normals!\n");
            return;
          }

          // Create a bool vector of processed point indices, and initialize it to false
          std::vector<bool> processed (cloud.points.size (), false);

          std::vector<int> nn_indices;
          std::vector<float> nn_distances;
          // Process all points in the indices vector
          int size = static_cast<int> (cloud.points.size ());
          for (int i = 0; i < size; ++i)
          {
            if (processed[i])
              continue;

            std::vector<unsigned int> seed_queue;
            int sq_idx = 0;
            seed_queue.push_back (i);

            processed[i] = true;

            while (sq_idx < static_cast<int> (seed_queue.size ()))
            {

              if (normals.points[seed_queue[sq_idx]].curvature > curvature_threshold)
              {
                sq_idx++;
                continue;
              }

              // Search for sq_idx
              if (!tree->radiusSearch (seed_queue[sq_idx], tolerance, nn_indices, nn_distances))
              {
                sq_idx++;
                continue;
              }

              for (size_t j = 1; j < nn_indices.size (); ++j) // nn_indices[0] should be sq_idx
              {
                if (processed[nn_indices[j]]) // Has this point been processed before ?
                  continue;

                if (normals.points[nn_indices[j]].curvature > curvature_threshold)
                {
                  continue;
                }

                //processed[nn_indices[j]] = true;
                // [-1;1]

                double dot_p = normals.points[seed_queue[sq_idx]].normal[0] * normals.points[nn_indices[j]].normal[0]
                    + normals.points[seed_queue[sq_idx]].normal[1] * normals.points[nn_indices[j]].normal[1] + normals.points[seed_queue[sq_idx]].normal[2]
                    * normals.points[nn_indices[j]].normal[2];

                if (fabs (acos (dot_p)) < eps_angle)
                {
                  processed[nn_indices[j]] = true;
                  seed_queue.push_back (nn_indices[j]);
                }
              }

              sq_idx++;
            }

            // If this queue is satisfactory, add to the clusters
            if (seed_queue.size () >= min_pts_per_cluster && seed_queue.size () <= max_pts_per_cluster)
            {
              pcl::PointIndices r;
              r.indices.resize (seed_queue.size ());
              for (size_t j = 0; j < seed_queue.size (); ++j)
                r.indices[j] = seed_queue[j];

              std::sort (r.indices.begin (), r.indices.end ());
              r.indices.erase (std::unique (r.indices.begin (), r.indices.end ()), r.indices.end ());
              clusters.push_back (r); // We could avoid a copy by working directly in the vector
            }
          }
        }

      void computeClutterCueAtOnce ();

      virtual bool
      handlingNormals (boost::shared_ptr<GHVRecognitionModel<ModelT> > & recog_model, int i, int object_models_size);

      virtual bool
      addModel (int i, boost::shared_ptr<GHVRecognitionModel<ModelT> > & recog_model);

      //Performs smooth segmentation of the scene cloud and compute the model cues
      virtual void
      initialize ();

      pcl::PointCloud<pcl::Normal>::Ptr scene_normals_;
      bool scene_and_normals_set_from_outside_;
      std::vector<std::string> object_ids_;
      std::vector<float> extra_weights_;

      //class attributes
      typedef typename pcl::NormalEstimation<SceneT, pcl::Normal> NormalEstimator_;
      pcl::PointCloud<pcl::PointXYZL>::Ptr clusters_cloud_;
      int max_label_clusters_cloud_;
      pcl::PointCloud<pcl::PointXYZRGBA>::Ptr clusters_cloud_rgb_;
      pcl::PointCloud<pcl::Normal>::Ptr scene_normals_for_clutter_term_;

      std::vector<int> complete_cloud_occupancy_by_RM_;

      std::vector<double> duplicates_by_RM_weighted_;
      std::vector<double> duplicates_by_RM_weighted_not_capped;
      std::vector<int> explained_by_RM_; //represents the points of scene_cloud_ that are explained by the recognition models
      std::vector<double> explained_by_RM_distance_weighted; //represents the points of scene_cloud_ that are explained by the recognition models
      std::vector<int> explained_by_RM_model; //id of the model explaining the point
      std::vector< std::stack<std::pair<int, float>, std::vector<std::pair<int, float> > > > previous_explained_by_RM_distance_weighted; //represents the points of scene_cloud_ that are explained by the recognition models
      std::vector<double> unexplained_by_RM_neighboorhods; //represents the points of scene_cloud_ that are not explained by the active hypotheses in the neighboorhod of the recognition models
      std::vector<boost::shared_ptr<GHVRecognitionModel<ModelT> > > recognition_models_;
      //std::vector<size_t> indices_;
      std::vector<bool> valid_model_;

      double previous_explained_value;
      double previous_duplicity_;
      int previous_duplicity_complete_models_;
      double previous_bad_info_;
      double previous_unexplained_;

      GHVSAModel<ModelT, SceneT> best_seen_;
      float initial_temp_;

      //conflict graph stuff
      int n_cc_;
      std::vector<std::vector<int> > cc_;

      std::vector<std::vector<boost::shared_ptr<GHVRecognitionModel<ModelT> > > > points_explained_by_rm_; //if inner size > 1, conflict

      //mahalanobis stuff
      Eigen::MatrixXf inv_covariance_;
      Eigen::VectorXf mean_;

      void
      setPreviousBadInfo (double f)
      {
        previous_bad_info_ = f;
      }

      double
      getPreviousBadInfo () const
      {
        return previous_bad_info_;
      }

      void
      setPreviousExplainedValue (double v)
      {
        previous_explained_value = v;
      }

      void
      setPreviousDuplicity (double v)
      {
        previous_duplicity_ = v;
      }

      void
      setPreviousDuplicityCM (int v)
      {
        previous_duplicity_complete_models_ = v;
      }

      void
      setPreviousUnexplainedValue (double v)
      {
        previous_unexplained_ = v;
      }

      double
      getPreviousUnexplainedValue () const
      {
        return previous_unexplained_;
      }

      double
      getExplainedValue () const
      {
        return previous_explained_value;
      }

      double
      getDuplicity () const
      {
        return previous_duplicity_;
      }

      int
      getDuplicityCM () const
      {
        return previous_duplicity_complete_models_;
      }

      double
      getExplainedByIndices (const std::vector<int> & indices,
                             const std::vector<float> & explained_values,
                             const std::vector<double> & explained_by_RM,
                             std::vector<int> & indices_to_update_in_RM_local);

      void
      getExplainedByRM (std::vector<double> & explained_by_rm)
      {
        explained_by_rm = explained_by_RM_distance_weighted;
      }

      void
      getUnexplainedByRM (std::vector<double> & explained_by_rm)
      {
        explained_by_rm = unexplained_by_RM_neighboorhods;
      }

      void
      updateUnexplainedVector (const std::vector<int> & unexplained_, const std::vector<float> & unexplained_distances, std::vector<double> & unexplained_by_RM,
                               std::vector<int> & explained, std::vector<int> & explained_by_RM, float val)
      {
          double add_to_unexplained = 0.0;

          for (size_t i = 0; i < unexplained_.size (); i++)
          {

            bool prev_unexplained = (unexplained_by_RM[unexplained_[i]] > 0) && (explained_by_RM[unexplained_[i]] == 0);
            unexplained_by_RM[unexplained_[i]] += val * unexplained_distances[i];

            if (val < 0) //the hypothesis is being removed
            {
              if (prev_unexplained)
              {
                //decrease by 1
                add_to_unexplained -= unexplained_distances[i];
              }
            }
            else //the hypothesis is being added and unexplains unexplained_[i], so increase by 1 unless its explained by another hypothesis
            {
              if (explained_by_RM[unexplained_[i]] == 0)
                add_to_unexplained += unexplained_distances[i];
            }
          }

          for (size_t i = 0; i < explained.size (); i++)
          {
            if (val < 0)
            {
              //the hypothesis is being removed, check that there are no points that become unexplained and have clutter unexplained hypotheses
              if ((explained_by_RM[explained[i]] == 0) && (unexplained_by_RM[explained[i]] > 0))
                add_to_unexplained += unexplained_by_RM[explained[i]]; //the points become unexplained
            }
            else
            {
              //std::cout << "being added..." << add_to_unexplained << " " << unexplained_by_RM[explained[i]] << std::endl;
              if ((explained_by_RM[explained[i]] == 1) && (unexplained_by_RM[explained[i]] > 0))
              { //the only hypothesis explaining that point
                add_to_unexplained -= unexplained_by_RM[explained[i]]; //the points are not unexplained any longer because this hypothesis explains them
              }
            }
          }

          //std::cout << add_to_unexplained << std::endl;
          previous_unexplained_ += add_to_unexplained;
      }

      void
      updateExplainedVector (const std::vector<int> & vec, const std::vector<float> & vec_float, std::vector<int> & explained_,
                             std::vector<double> & explained_by_RM_distance_weighted__not_used, float sign, int model_id);

      void
      updateCMDuplicity (const std::vector<int> & vec, std::vector<int> & occupancy_vec, float sign);

      double
      getTotalExplainedInformation (const std::vector<int> & explained_, const std::vector<double> & explained_by_RM_distance_weighted, double &duplicity_);

      double
      getTotalBadInformation (std::vector<boost::shared_ptr<GHVRecognitionModel<ModelT> > > & recog_models)
      {
        double bad_info = 0;
        for (size_t i = 0; i < recog_models.size (); i++)
          bad_info += recog_models[i]->outliers_weight_ * static_cast<double> (recog_models[i]->bad_information_);

        return bad_info;
      }

      double
      getUnexplainedInformationInNeighborhood (std::vector<double> & unexplained, std::vector<int> & explained)
      {
        double unexplained_sum = 0.f;
        for (size_t i = 0; i < unexplained.size (); i++)
        {
          if (unexplained[i] > 0 && explained[i] == 0)
            unexplained_sum += unexplained[i];
        }

        return unexplained_sum;
      }

      mets::gol_type
      evaluateSolution (const std::vector<bool> & active, int changed);

      void
      SAOptimize (std::vector<int> & cc_indices, std::vector<bool> & sub_solution);

      void
      fill_structures (std::vector<int> & cc_indices, std::vector<bool> & sub_solution, GHVSAModel<ModelT, SceneT> & model);

      void
      clear_structures ();

      double
      countActiveHypotheses (const std::vector<bool> & sol);

      double
      countPointsOnDifferentPlaneSides (const std::vector<bool> & sol, bool print=false);

      boost::shared_ptr<GHVCostFunctionLogger<ModelT,SceneT> > cost_logger_;

      void
      computeRGBHistograms (const std::vector<Eigen::Vector3f> & rgb_values, Eigen::MatrixXf & rgb,
                               int dim = 3, float min = 0.f, float max = 255.f, bool soft = false);

      void
      specifyRGBHistograms (Eigen::MatrixXf & src, Eigen::MatrixXf & dst, Eigen::MatrixXf & lookup, int dim = 3);

      void
      computeGSHistogram (const std::vector<float> & hsv_values, Eigen::MatrixXf & histogram, int hist_size = 255);

      std::vector<PlaneModel<ModelT> > planar_models_;
      std::map<int, int> model_to_planar_model_;

      typename boost::shared_ptr<pcl::octree::OctreePointCloudSearch<SceneT> > octree_scene_downsampled_;

      int min_contribution_;
      bool LS_short_circuit_;
      std::vector<std::vector<float> > points_one_plane_sides_;

      boost::function<void (const std::vector<bool> &, float, int)> visualize_cues_during_logger_;
      int visualize_go_cues_;

      void visualizeGOCues(const std::vector<bool> & active_solution, float cost, int times_eval);

      boost::shared_ptr<pcl::visualization::PCLVisualizer> vis_go_cues_;

      std::vector<pcl::PointCloud<pcl::PointXYZL>::Ptr> models_smooth_faces_;

      void specifyColor(int i, Eigen::MatrixXf & lookup, boost::shared_ptr<GHVRecognitionModel<ModelT> > & recog_model);

      std::vector<float> scene_curvature_;
      std::vector<Eigen::Vector3f> scene_LAB_values_;
      std::vector<Eigen::Vector3f> scene_RGB_values_;
      std::vector<float> scene_GS_values_;
      int color_space_;
      bool visualize_accepted_;
      typedef pcl::PointCloud<ModelT> CloudM;
      typedef pcl::PointCloud<SceneT> CloudS;
      typedef typename pcl::traits::fieldList<typename CloudS::PointType>::type FieldListS;
      typedef typename pcl::traits::fieldList<typename CloudM::PointType>::type FieldListM;

      float getCurvWeight(float p_curvature);

      int max_threads_;

      std::vector<std::string> ply_paths_;
      std::vector<vtkSmartPointer <vtkTransform> > poses_ply_;

      float t_cues_, t_opt_;
      int number_of_visible_points_;


      //compute mahalanobis distance
      float mahalanobis(Eigen::VectorXf & mu, Eigen::VectorXf & x, Eigen::MatrixXf & inv_cov)
      {
          float product = (x - mu).transpose() * inv_cov * (x - mu);
          return sqrt(product);
      }

    public:

      GHV (const Parameter &p=Parameter()) : HypothesisVerification<ModelT, SceneT> (p)
      {
        param_ = p;
        initial_temp_ = 1000;
        requires_normals_ = false;

        min_contribution_ = 0;
        LS_short_circuit_ = false;
        visualize_go_cues_ = 0; //0 - No visualization, 1 - accepted moves
        color_space_ = 0;
        visualize_accepted_ = false;

        max_threads_ = 1;
        scene_and_normals_set_from_outside_ = false;
      }

      void setMeanAndCovariance(Eigen::VectorXf & mean, Eigen::MatrixXf & cov)
      {
          mean_ = mean;
          inv_covariance_ = cov;
      }

      void setSceneAndNormals(typename pcl::PointCloud<SceneT>::Ptr & scene,
                              typename pcl::PointCloud<pcl::Normal>::Ptr & scene_normals)
      {
         scene_cloud_downsampled_ = scene;
         scene_normals_ = scene_normals;
         scene_and_normals_set_from_outside_ = true;
      }

      int getNumberOfVisiblePoints()
      {
          return number_of_visible_points_;
      }

      float getCuesComputationTime()
      {
          return t_cues_;
      }

      float getOptimizationTime()
      {
          return t_opt_;
      }

      void setPlyPathsAndPoses(std::vector<std::string> & ply_paths_for_go, std::vector<vtkSmartPointer <vtkTransform> > & poses_ply)
      {
          ply_paths_ = ply_paths_for_go;
          poses_ply_ = poses_ply;
      }

      void setMaxThreads(int t)
      {
          max_threads_ = t;
      }

      void setVisualizeAccepted(bool b)
      {
          visualize_accepted_ = b;
      }

      //0 for LAB (specifying L), 1 for RGB (specifying all)
      void setColorSpace(int cs)
      {
          std::cout << "called color space" << cs << std::endl;
          color_space_ = cs;
      }

      void setSmoothFaces(std::vector<pcl::PointCloud<pcl::PointXYZL>::Ptr> & aligned_smooth_faces)
      {
          models_smooth_faces_ = aligned_smooth_faces;
      }

      void setVisualizeGoCues(int v)
      {
        visualize_go_cues_ = v;
      }

      void setLSShortCircuit(bool b)
      {
          LS_short_circuit_ = b;
      }

      void setNormalsForClutterTerm(pcl::PointCloud<pcl::Normal>::Ptr & normals)
      {
          scene_normals_for_clutter_term_ = normals;
      }

      void addPlanarModels(std::vector<PlaneModel<ModelT> > & models);

      void
      setSmoothSegParameters (float t_eps, float curv_t, float dist_t, int min_points = 20)
      {
        param_.eps_angle_threshold_ = t_eps;
        param_.min_points_ = min_points;
        param_.curvature_threshold_ = curv_t;
        param_.cluster_tolerance_ = dist_t;
      }

      void
      setObjectIds (std::vector<std::string> & ids)
      {
        object_ids_ = ids;
      }

      void
      writeToLog (std::ofstream & of, bool all_costs_ = false)
      {
        cost_logger_->writeToLog (of);
        if (all_costs_)
          cost_logger_->writeEachCostToLog (of);
      }

      void setMinContribution(int min)
      {
          min_contribution_ = min;
      }

      /*void logCosts() {
       cost_logger_.reset(new CostFunctionLogger());
       }*/

      pcl::PointCloud<pcl::PointXYZL>::Ptr
      getSmoothClusters ()
      {
        return clusters_cloud_;
      }

      pcl::PointCloud<pcl::PointXYZRGBA>::Ptr
      getSmoothClustersRGBCloud ()
      {
        return clusters_cloud_rgb_;
      }

      float
      getResolution ()
      {
        return param_.resolution_;
      }

      void
      setRequiresNormals (bool b)
      {
        requires_normals_ = b;
      }


      void
      verify ();

      void
      setInitialTemp (float t)
      {
        initial_temp_ = t;
      }

      //Same length as the recognition models
      void
      setExtraWeightVectorForInliers (std::vector<float> & weights)
      {
        extra_weights_.clear ();
        extra_weights_ = weights;
      }

      virtual
      bool add_planes_is_posssible() const
      {
          return true;
      }

      virtual
      bool uses_3D() const
      {
          return false;
      }

      void
      getOutliersForAcceptedModels(std::vector< pcl::PointCloud<pcl::PointXYZ>::Ptr > & outliers_cloud);

      void
      getOutliersForAcceptedModels(std::vector< pcl::PointCloud<pcl::PointXYZ>::Ptr > & outliers_cloud_color,
                                   std::vector< pcl::PointCloud<pcl::PointXYZ>::Ptr > & outliers_cloud_3d);

    };
}

#endif //FAAT_PCL_GHV_H_
