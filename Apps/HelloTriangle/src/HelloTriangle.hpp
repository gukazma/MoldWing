/*
 *  MoldWing - HelloTriangle Sample
 *  Based on DiligentEngine Tutorial01
 */

#pragma once

#include "SampleBase.hpp"

namespace MoldWing
{

class HelloTriangle final : public Diligent::SampleBase
{
public:
    virtual void Initialize(const Diligent::SampleInitInfo& InitInfo) override final;
    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

    virtual const Diligent::Char* GetSampleName() const override final
    {
        return "MoldWing: Hello Triangle";
    }

private:
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
};

} // namespace MoldWing
