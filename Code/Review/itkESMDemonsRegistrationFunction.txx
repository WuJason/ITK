/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkESMDemonsRegistrationFunction.txx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef __itkESMDemonsRegistrationFunction_txx
#define __itkESMDemonsRegistrationFunction_txx

#include "itkESMDemonsRegistrationFunction.h"
#include "itkExceptionObject.h"
#include "vnl/vnl_math.h"

namespace itk {

/**
 * Default constructor
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::ESMDemonsRegistrationFunction()
{

  RadiusType r;
  unsigned int j;
  for( j = 0; j < ImageDimension; j++ )
    {
    r[j] = 0;
    }
  this->SetRadius(r);

  m_TimeStep = 1.0;
  m_DenominatorThreshold = 1e-9;
  m_IntensityDifferenceThreshold = 0.001;
  m_MaximumUpdateStepLength = 0.5;
  
  this->SetMovingImage(NULL);
  this->SetFixedImage(NULL);
  m_FixedImageSpacing.Fill( 1.0 );
  m_FixedImageOrigin.Fill( 0.0 );
  m_FixedImageDirection.SetIdentity();
  m_Normalizer = 0.0;
  m_FixedImageGradientCalculator = GradientCalculatorType::New();
  m_MappedMovingImageGradientCalculator = MovingImageGradientCalculatorType::New();

  this->m_UseGradientType = Symmetric;

  typename DefaultInterpolatorType::Pointer interp =
    DefaultInterpolatorType::New();

  m_MovingImageInterpolator = static_cast<InterpolatorType*>(
    interp.GetPointer() );

  m_MovingImageWarper = WarperType::New();
  m_MovingImageWarper->SetInterpolator( m_MovingImageInterpolator );
  m_MovingImageWarper->SetEdgePaddingValue( NumericTraits<MovingPixelType>::max() );

  m_Metric = NumericTraits<double>::max();
  m_SumOfSquaredDifference = 0.0;
  m_NumberOfPixelsProcessed = 0L;
  m_RMSChange = NumericTraits<double>::max();
  m_SumOfSquaredChange = 0.0;
}


/*
 * Standard "PrintSelf" method.
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
void
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::PrintSelf(std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "UseGradientType: ";
  os << m_UseGradientType << std::endl;
  os << indent << "MaximumUpdateStepLength: ";
  os << m_MaximumUpdateStepLength << std::endl;

  os << indent << "MovingImageIterpolator: ";
  os << m_MovingImageInterpolator.GetPointer() << std::endl;
  os << indent << "FixedImageGradientCalculator: ";
  os << m_FixedImageGradientCalculator.GetPointer() << std::endl;
  os << indent << "MappedMovingImageGradientCalculator: ";
  os << m_MappedMovingImageGradientCalculator.GetPointer() << std::endl;
  os << indent << "DenominatorThreshold: ";
  os << m_DenominatorThreshold << std::endl;
  os << indent << "IntensityDifferenceThreshold: ";
  os << m_IntensityDifferenceThreshold << std::endl;

  os << indent << "Metric: ";
  os << m_Metric << std::endl;
  os << indent << "SumOfSquaredDifference: ";
  os << m_SumOfSquaredDifference << std::endl;
  os << indent << "NumberOfPixelsProcessed: ";
  os << m_NumberOfPixelsProcessed << std::endl;
  os << indent << "RMSChange: ";
  os << m_RMSChange << std::endl;
  os << indent << "SumOfSquaredChange: ";
  os << m_SumOfSquaredChange << std::endl;

}

/**
 *
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
void
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::SetIntensityDifferenceThreshold(double threshold)
{
  m_IntensityDifferenceThreshold = threshold;
}

/**
 *
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
double
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::GetIntensityDifferenceThreshold() const
{
  return m_IntensityDifferenceThreshold;
}

/**
 * Set the function state values before each iteration
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
void
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::InitializeIteration()
{
  if( !this->GetMovingImage() || !this->GetFixedImage()
      || !m_MovingImageInterpolator )
    {
    itkExceptionMacro(
       << "MovingImage, FixedImage and/or Interpolator not set" );
    }

  // cache fixed image information
  m_FixedImageOrigin  = this->GetFixedImage()->GetOrigin();
  m_FixedImageSpacing = this->GetFixedImage()->GetSpacing();
  m_FixedImageDirection = this->GetFixedImage()->GetDirection();

  // compute the normalizer
  if( m_MaximumUpdateStepLength > 0.0 )
    {
    m_Normalizer = 0.0;
    for( unsigned int k = 0; k < ImageDimension; k++ )
      {
      m_Normalizer += m_FixedImageSpacing[k] * m_FixedImageSpacing[k];
      }
    m_Normalizer *= m_MaximumUpdateStepLength * m_MaximumUpdateStepLength /
      static_cast<double>( ImageDimension );
    }
  else
    {
    // set it to minus one to denote a special case
    // ( unrestricted update length )
    m_Normalizer = -1.0;
    }
  
  // setup gradient calculator
  m_FixedImageGradientCalculator->SetInputImage( this->GetFixedImage() );
  m_MappedMovingImageGradientCalculator->SetInputImage( this->GetMovingImage() );

  // Compute warped moving image
  m_MovingImageWarper->SetOutputOrigin( this->GetFixedImage()->GetOrigin() );
  m_MovingImageWarper->SetOutputSpacing( this->GetFixedImage()->GetSpacing() );
  m_MovingImageWarper->SetOutputDirection( this->GetFixedImage()->GetDirection() );
  m_MovingImageWarper->SetInput( this->GetMovingImage() );
  m_MovingImageWarper->SetDeformationField( this->GetDeformationField() );
  m_MovingImageWarper->GetOutput()->SetRequestedRegion( this->GetDeformationField()->GetRequestedRegion() );
  m_MovingImageWarper->Update();
  
  // setup moving image interpolator for further access
  m_MovingImageInterpolator->SetInputImage( this->GetMovingImage() );
  
  // initialize metric computation variables
  m_SumOfSquaredDifference  = 0.0;
  m_NumberOfPixelsProcessed = 0L;
  m_SumOfSquaredChange      = 0.0;
}


/**
 * Compute update at a non boundary neighbourhood
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
typename ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::PixelType
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::ComputeUpdate(const NeighborhoodType &it, void * gd,
                const FloatOffsetType& itkNotUsed(offset))
{

  GlobalDataStruct *globalData = (GlobalDataStruct *)gd;
  PixelType update;
  IndexType FirstIndex = this->GetFixedImage()->GetLargestPossibleRegion().GetIndex();
  IndexType LastIndex = this->GetFixedImage()->GetLargestPossibleRegion().GetIndex() + 
     this->GetFixedImage()->GetLargestPossibleRegion().GetSize();
  
  const IndexType index = it.GetIndex();
  
  // Get fixed image related information
  // Note: no need to check if the index is within
  // fixed image buffer. This is done by the external filter.
  const double fixedValue = static_cast<double>(
     this->GetFixedImage()->GetPixel( index ) );
  

  // Get moving image related information
  // check if the point was mapped outside of the moving image using
  // the "special value" NumericTraits<MovingPixelType>::max()
  MovingPixelType movingPixValue
     = m_MovingImageWarper->GetOutput()->GetPixel( index );

  if( movingPixValue == NumericTraits <MovingPixelType>::max() )
    {
    update.Fill( 0.0 );
    return update;
    }


  
  const double movingValue = static_cast<double>( movingPixValue );

  CovariantVectorType usedGradientTimes2;
  
  if( (this->m_UseGradientType==Symmetric) || 
      (this->m_UseGradientType==WarpedMoving) )
    {
    // we don't use a CentralDifferenceImageFunction here to be able to
    // check for NumericTraits<MovingPixelType>::max()
    CovariantVectorType warpedMovingGradient;
    IndexType tmpIndex = index;
    for( unsigned int dim = 0; dim < ImageDimension; dim++ )
      {
      // bounds checking
      if( FirstIndex[dim]==LastIndex[dim]
          || index[dim]<FirstIndex[dim]
          || index[dim]>=LastIndex[dim] )
        {
        warpedMovingGradient[dim] = 0.0;
        continue;
        }
      else if ( index[dim] == FirstIndex[dim] )
        {
        // compute derivative
        tmpIndex[dim] += 1;
        movingPixValue = m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex );
        if( movingPixValue == NumericTraits <MovingPixelType>::max() )
          {
          // weird crunched border case
          warpedMovingGradient[dim] = 0.0;
          }
        else
          {
          // forward difference
          warpedMovingGradient[dim] = static_cast<double>( movingPixValue ) - movingValue;
          warpedMovingGradient[dim] /= m_FixedImageSpacing[dim]; 
          }
        tmpIndex[dim] -= 1;
        continue;
        }
      else if ( index[dim] == (LastIndex[dim]-1) )
        {
        // compute derivative
        tmpIndex[dim] -= 1;
        movingPixValue = m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex );
        if( movingPixValue == NumericTraits<MovingPixelType>::max() )
          {
          // weird crunched border case
          warpedMovingGradient[dim] = 0.0;
          }
        else
          {
          // backward difference
          warpedMovingGradient[dim] = movingValue - static_cast<double>( movingPixValue );
          warpedMovingGradient[dim] /= m_FixedImageSpacing[dim]; 
          }
        tmpIndex[dim] += 1;
        continue;
        }
      
      
      // compute derivative
      tmpIndex[dim] += 1;
      movingPixValue = m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex );
      if ( movingPixValue == NumericTraits
           <MovingPixelType>::max() )
        {
        // backward difference
        warpedMovingGradient[dim] = movingValue;
         
        tmpIndex[dim] -= 2;
        movingPixValue = m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex );
        if( movingPixValue == NumericTraits<MovingPixelType>::max() )
          {
          // weird crunched border case
          warpedMovingGradient[dim] = 0.0;
          }
        else
          {
          // backward difference
          warpedMovingGradient[dim] -= static_cast<double>(
               m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex ) );
            
          warpedMovingGradient[dim] /= m_FixedImageSpacing[dim];
          }
        }
      else
        {
        warpedMovingGradient[dim] = static_cast<double>( movingPixValue );
        
        tmpIndex[dim] -= 2;
        movingPixValue = m_MovingImageWarper->GetOutput()->GetPixel( tmpIndex );
        if ( movingPixValue == NumericTraits<MovingPixelType>::max() )
          {
          // forward difference
          warpedMovingGradient[dim] -= movingValue;
          warpedMovingGradient[dim] /= m_FixedImageSpacing[dim];
          }
        else
          {
          // normal case, central difference
          warpedMovingGradient[dim] -= static_cast<double>( movingPixValue );
          warpedMovingGradient[dim] *= 0.5 / m_FixedImageSpacing[dim];
          }
        }
      tmpIndex[dim] += 1;
      }

    if( this->m_UseGradientType == Symmetric )
      {
      const CovariantVectorType fixedGradient
        = m_FixedImageGradientCalculator->EvaluateAtIndex( index );
       
      usedGradientTimes2 = fixedGradient + warpedMovingGradient;
      }
    else if (this->m_UseGradientType==WarpedMoving)
      {
      usedGradientTimes2 = warpedMovingGradient + warpedMovingGradient;
      }
    else
      {
      itkExceptionMacro(<<"Unknown gradient type");
      }
    }
  else if (this->m_UseGradientType==Fixed)
    {
    const CovariantVectorType fixedGradient
      = m_FixedImageGradientCalculator->EvaluateAtIndex( index );
     
    usedGradientTimes2 = fixedGradient + fixedGradient;
    }
  else if (this->m_UseGradientType==MappedMoving)
    {
    PointType mappedPoint;
    //
    // FIXME: This will not work with OrientedImages.  Conversions of index to
    // physical points should be delegated to the image class.
    //
#ifdef ITK_IMAGE_BEHAVES_AS_ORIENTED_IMAGE
#ifndef _WIN32
#warning "This class does not work when ITK_IMAGE_BEHAVES_AS_ORIENTED_IMAGE is turned on and the FixedImage Direction is not identity"
#endif
    bool directionIsIdentity = true;
    for( unsigned int i = 0; i < ImageDimension; i++ )
      {
      for( unsigned int j = 0; j < ImageDimension; j++ )
        {
        if (i == j)
          {
          if (m_FixedImageDirection[i][j] != 1.0)
            {
            directionIsIdentity = false;
            }
          }
        else
          {
          if (m_FixedImageDirection[i][j] != 0.0)
            {
            directionIsIdentity = false;
            }
          }
        }
      }
    if (!directionIsIdentity)
      {
      itkExceptionMacro("This class does not work when ITK_IMAGE_BEHAVES_AS_ORIENTED_IMAGE is turned on");
      }
#endif
    for( unsigned int j = 0; j < ImageDimension; j++ )
      {
      mappedPoint[j] = double( index[j] ) * m_FixedImageSpacing[j] + 
         m_FixedImageOrigin[j];
      mappedPoint[j] += it.GetCenterPixel()[j];
      }
     
    const CovariantVectorType mappedMovingGradient
      = m_MappedMovingImageGradientCalculator->Evaluate( mappedPoint );
     
    usedGradientTimes2 = mappedMovingGradient + mappedMovingGradient;
    }
  else
    {
    itkExceptionMacro(<<"Unknown gradient type");
    }


  
  /**
   * Compute Update.
   * We avoid the mismatch in units between the two terms. 
   * and avoid large step using a normalization term.
   */
  
  const double usedGradientTimes2SquaredMagnitude =
     usedGradientTimes2.GetSquaredNorm();
     
  const double speedValue = fixedValue - movingValue;
  if ( vnl_math_abs(speedValue) < m_IntensityDifferenceThreshold )
    {
    update.Fill( 0.0 );
    }
  else
    {  
    double denom;
    if(  m_Normalizer > 0.0 )
      {
      // "ITK-Thirion" normalization
      denom =  usedGradientTimes2SquaredMagnitude + (vnl_math_sqr(speedValue)/m_Normalizer);
      }
    else
      {
      // least square solution of the system
      denom =  usedGradientTimes2SquaredMagnitude;
      }
        
        
    if( denom < m_DenominatorThreshold )
      {
      update.Fill( 0.0 );
      }
    else
      {
      const double factor = 2.0 * speedValue / denom;
           
      for( unsigned int j = 0; j < ImageDimension; j++ )
        {
        update[j] = factor * usedGradientTimes2[j];
        }
      }
    }

  // WARNING!! We compute the global data without taking into account the current update step.
  // There are several reasons for that: If an exponential, a smoothing or any other operation
  // is applied on the update field, we cannot compute the newMappedCenterPoint here; and even
  // if we could, this would be an often unnecessary time-consuming task.
  if ( globalData )
    {
    globalData->m_SumOfSquaredDifference += vnl_math_sqr( speedValue );
    globalData->m_NumberOfPixelsProcessed += 1;
    globalData->m_SumOfSquaredChange += update.GetSquaredNorm();
    }

  return update;
}


/**
 * Update the metric and release the per-thread-global data.
 */
template <class TFixedImage, class TMovingImage, class TDeformationField>
void
ESMDemonsRegistrationFunction<TFixedImage,TMovingImage,TDeformationField>
::ReleaseGlobalDataPointer( void *gd ) const
{
  GlobalDataStruct * globalData = (GlobalDataStruct *) gd;

  m_MetricCalculationLock.Lock();
  m_SumOfSquaredDifference += globalData->m_SumOfSquaredDifference;
  m_NumberOfPixelsProcessed += globalData->m_NumberOfPixelsProcessed;
  m_SumOfSquaredChange += globalData->m_SumOfSquaredChange;
  if( m_NumberOfPixelsProcessed )
    {
    m_Metric = m_SumOfSquaredDifference / 
               static_cast<double>( m_NumberOfPixelsProcessed ); 
    m_RMSChange = vcl_sqrt( m_SumOfSquaredChange / 
               static_cast<double>( m_NumberOfPixelsProcessed ) ); 
    }
  m_MetricCalculationLock.Unlock();

  delete globalData;
}

} // end namespace itk

#endif
