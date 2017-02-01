/******************************************************************************
 * Copyright (c) 2012 Aitor Aldoma, Thomas Faeulhammer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ******************************************************************************/

#ifndef V4R_ZBUFFERING_H_
#define V4R_ZBUFFERING_H_

#include <boost/dynamic_bitset.hpp>
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/common/io.h>
#include <v4r/core/macros.h>
#include <v4r/common/camera.h>


namespace v4r
{

/**
     * \brief Class to reason about occlusions
     * \author Thomas Faeulhammer, Aitor Aldoma
     */
template<typename PointT>
class V4R_EXPORTS ZBuffering
{
public:
    typedef boost::shared_ptr< ZBuffering<PointT> > Ptr;

    class Parameter
    {
    public:
        int u_margin_, v_margin_;
        bool compute_focal_length_;
        bool do_smoothing_; ///< tries to fill holes by dilating points over neighboring pixel
        bool do_noise_filtering_;
        float inlier_threshold_;
        int smoothing_radius_;
        bool force_unorganized_; ///< re-projects points by given camera intrinsics even if point cloud is already organized
        Parameter(
                int u_margin = 0,
                int v_margin = 0,
                bool compute_focal_length = false,
                bool do_smoothing = true,
                bool do_noise_filtering = false,
                float inlier_threshold = 0.01f,
                int smoothing_radius = 1,
                bool force_unorganized = false) :
            u_margin_(u_margin), v_margin_(v_margin),
            compute_focal_length_(compute_focal_length), do_smoothing_(do_smoothing),
            do_noise_filtering_ (do_noise_filtering), inlier_threshold_(inlier_threshold),
            smoothing_radius_(smoothing_radius), force_unorganized_ (force_unorganized)
        {}
    }param_;

private:
    std::vector<float> depth_;
    std::vector<int> kept_indices_;
    Camera::ConstPtr cam_;   ///< camera parameters
    Eigen::MatrixXi index_map_; ///< saves for each pixel which indices of the input cloud it represents. Non-occupied pixels are labelled with index -1.

public:

    ZBuffering (const Camera::ConstPtr cam, const Parameter &p=Parameter()) : param_(p), cam_ (cam) { }

    void setCamera(const Camera::ConstPtr cam)
    {
        cam_ = cam;
    }

    void renderPointCloud(const typename pcl::PointCloud<PointT> &cloud, typename pcl::PointCloud<PointT> & rendered_view);

    void getKeptIndices(std::vector<int> &indices) const
    {
        indices = kept_indices_;
    }

    /**
     * @brief getIndexMap
     * @return each pixel indicates which point of the input cloud it represents. Non-occupied pixels are labelled with index -1.
     */
    Eigen::MatrixXi
    getIndexMap() const
    {
        return index_map_;
    }
};
}

#endif
