#include "SBMFontTextBlock.h"
#include "SceneViewport.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"

class FBMFontTextBlockViewportClient : public FViewportClient, public FGCObject
{
public:
	FBMFontTextBlockViewportClient();

	void PrepareToDraw(FViewport* Viewport, const TArray<FWrappedStringElement> &InText, const UFont *InFont, const FLinearColor &InColor, const FVector2D &InShadowOffset, const FLinearColor &InShadowColor, const FMargin &InMargin, float InScale, float InDesiredLineHeight);

	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	
	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(Font);
	}
private:
	TArray<FWrappedStringElement> Text;
	const UFont* Font;
	FLinearColor Color;
	FVector2D ShadowOffset;
	FLinearColor ShadowColor;
	FMargin Margin;
	FVector2D Scale;
	float DesiredLineHeight;
};

FBMFontTextBlockViewportClient::FBMFontTextBlockViewportClient()
	: Font(nullptr)
	, DesiredLineHeight(0.f)
{
}

void FBMFontTextBlockViewportClient::PrepareToDraw(FViewport* Viewport, const TArray<FWrappedStringElement> &InText, const UFont *InFont, const FLinearColor &InColor, const FVector2D &InShadowOffset, const FLinearColor &InShadowColor, const FMargin &InMargin, float InScale, float InDesiredLineHeight)
{
	Text = InText;
	Font = InFont;
	Color = InColor;
	ShadowColor = InShadowColor;
	ShadowOffset = InShadowOffset;
	Margin = InMargin;
	Scale = FVector2D(InScale, InScale);
	DesiredLineHeight = InDesiredLineHeight;

	Viewport->Invalidate();
	Viewport->InvalidateDisplay();
}

void FBMFontTextBlockViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	Canvas->Clear(FLinearColor::Transparent);

	if (Text.Num() > 0 && Font)
	{
		ShadowOffset *= Scale;
		float PosX = Margin.Left + ShadowOffset.X < 0 ? -ShadowOffset.X : 0;
		float PosY = Margin.Top + ShadowOffset.Y < 0 ? -ShadowOffset.Y : 0;
		FVector2D CanvasPos = FVector2D(PosX, PosY);
		for (const FWrappedStringElement& Line : Text)
		{
			FCanvasTextItem TextItem(CanvasPos, FText::FromString(Line.Value), Font, Color);
			TextItem.EnableShadow(ShadowColor, ShadowOffset);
			TextItem.BlendMode = SE_BLEND_Translucent;
			TextItem.Scale = Scale;

			Canvas->DrawItem(TextItem);

			CanvasPos.Y += FMath::Max(Line.LineExtent.Y, DesiredLineHeight) * Scale.Y;
		}
	}
}

void SBMFontTextBlock::FWrappingCache::UpdateIfNeeded(const FText& InText, const UFont* InFont, float InWrapTextAt, float InMinDesiredWidth, float InDesiredLineHeight)
{
	bool bTextIsIdentical = TextShot.IdenticalTo(InText) && TextShot.IsDisplayStringEqualTo(InText);
	if (!bTextIsIdentical || Font != InFont || WrapTextAt != InWrapTextAt || MinDesiredWidth != InMinDesiredWidth || DesiredLineHeight != InDesiredLineHeight)
 	{
		TextShot = FTextSnapshot(InText);
		Font = InFont;
		WrapTextAt = InWrapTextAt;
		DesiredLineHeight = InDesiredLineHeight;
		MinDesiredWidth = InMinDesiredWidth;
		WrappedText.Empty();
		WrappedSize = FVector2D::ZeroVector;

		if (!InText.IsEmpty() && InFont)
		{
			if (InWrapTextAt > 0.0f)
			{
				FTextSizingParameters TextSizingParameters(0.0f, 0.0f, InWrapTextAt, 0.0f, InFont);
				FCanvasWordWrapper Wrapper;
				UCanvas::WrapString(Wrapper, TextSizingParameters, 0.0f, *InText.ToString(), WrappedText);
				WrappedSize.X = InWrapTextAt;
				for (const FWrappedStringElement &Line : WrappedText)
				{
					WrappedSize.X = FMath::Max(WrappedSize.X, Line.LineExtent.X);
					WrappedSize.Y += FMath::Max(Line.LineExtent.Y, DesiredLineHeight);
				}
			}
			else
			{
				FTextSizingParameters TextSizingParameters(InFont, 1.f, 1.f);
				UCanvas::CanvasStringSize(TextSizingParameters, *InText.ToString());
				FWrappedStringElement Line = FWrappedStringElement(*InText.ToString(), TextSizingParameters.DrawXL, TextSizingParameters.DrawYL);
				WrappedText.Add(Line);
				WrappedSize.X = FMath::Max(Line.LineExtent.X, MinDesiredWidth);
				WrappedSize.Y = FMath::Max(Line.LineExtent.Y, DesiredLineHeight);
			}
		}
	}
}

SBMFontTextBlock::~SBMFontTextBlock()
{
}

void SBMFontTextBlock::Construct(const FArguments& InArgs)
{
	Text = InArgs._Text;
	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	ShadowOffset = InArgs._ShadowOffset;
	ShadowColorAndOpacity = InArgs._ShadowColorAndOpacity;
	WrapTextAt = InArgs._WrapTextAt;
	AutoWrapText = InArgs._AutoWrapText;
	Margin = InArgs._Margin;
	DesiredLineHeight = InArgs._DesiredLineHeight;
	MinDesiredWidth = InArgs._MinDesiredWidth;
	WidgetCachedSize = FVector2D::ZeroVector;

	TSharedPtr<SViewport> ViewportWidget;
	
	ChildSlot
		[
			SAssignNew(ViewportWidget, SViewport)
			.EnableGammaCorrection(false)
			.EnableBlending(true)
			.ShowEffectWhenDisabled(true)
			.IgnoreTextureAlpha(false)
		];

	ViewportClient = MakeShareable(new FBMFontTextBlockViewportClient());
	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));

	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
}

int32 SBMFontTextBlock::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	WidgetCachedSize = AllottedGeometry.Size;

	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	ViewportClient->PrepareToDraw(Viewport.Get(), 
		WrappingCache.WrappedText, 
		Font.Get(nullptr), 
		ColorAndOpacity.Get(FLinearColor::White).GetColor(InWidgetStyle), 
		ShadowOffset.Get(FVector2D::ZeroVector),
		ShadowColorAndOpacity.Get(FLinearColor::Black),
		Margin.Get(FMargin()), 
		AllottedGeometry.Scale,
		DesiredLineHeight.Get(0.f));
	
		LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled);

	return LayerId;
}

void SBMFontTextBlock::CacheDesiredSize(float LayoutScaleMultiplier)
{
	// Text wrapping can either be used defined (WrapTextAt), automatic (bAutoWrapText and CachedSize), 
	// or a mixture of both. Take whichever has the smallest value (>1)
	const bool bAutoWrapText = AutoWrapText.Get(false);
	float WrappingWidth = WrapTextAt.Get(0.0f);
	if (bAutoWrapText && WidgetCachedSize.X >= 1.0f)
	{
		WrappingWidth = (WrappingWidth >= 1.0f) ? FMath::Min(WrappingWidth, WidgetCachedSize.X) : WidgetCachedSize.X;
	}
	WrappingWidth -= Margin.Get(FMargin()).GetTotalSpaceAlong<Orient_Horizontal>();
	WrappingWidth = FMath::Max(0.0f, WrappingWidth);

	WrappingCache.UpdateIfNeeded(Text.Get(FText::GetEmpty()), Font.Get(nullptr), WrappingWidth, MinDesiredWidth.Get(0.f), DesiredLineHeight.Get(0.f));
	
	SCompoundWidget::CacheDesiredSize(LayoutScaleMultiplier);
}

FVector2D SBMFontTextBlock::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D WrappedSize = WrappingCache.WrappedSize;
	FVector2D MarginSize = Margin.Get(FMargin()).GetDesiredSize();
	FVector2D ShadowSize = ShadowOffset.Get(FVector2D::ZeroVector).GetAbs() * LayoutScaleMultiplier;
	return WrappedSize + MarginSize + ShadowSize;
}

